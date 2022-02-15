#include "vulkanapi.h"

#if defined(TEMPEST_BUILD_VULKAN)

#include "vulkan/vdevice.h"
#include "vulkan/vswapchain.h"
#include "vulkan/vpipeline.h"
#include "vulkan/vbuffer.h"
#include "vulkan/vshader.h"
#include "vulkan/vfence.h"
#include "vulkan/vcommandpool.h"
#include "vulkan/vcommandbuffer.h"
#include "vulkan/vdescriptorarray.h"
#include "vulkan/vpipelinelay.h"
#include "vulkan/vtexture.h"
#include "vulkan/vaccelerationstructure.h"

#include "deviceallocator.h"
#include "shaderreflection.h"

#include "vulkan/vulkan_sdk.h"

#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <Tempest/PipelineLayout>
#include <Tempest/Application>

using namespace Tempest;
using namespace Tempest::Detail;

struct Tempest::VulkanApi::Impl : public VulkanInstance {
  using VulkanInstance::VulkanInstance;
  };

VulkanApi::VulkanApi(ApiFlags f) {
  impl.reset(new Impl(ApiFlags::Validation==(f&ApiFlags::Validation)));
  }

VulkanApi::~VulkanApi(){
  }

std::vector<AbstractGraphicsApi::Props> VulkanApi::devices() const {
  return impl->devices();
  }

AbstractGraphicsApi::Device *VulkanApi::createDevice(const char* gpuName) {
  return new VDevice(*impl,gpuName);
  }

void VulkanApi::destroy(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  delete dx;
  }

AbstractGraphicsApi::Swapchain *VulkanApi::createSwapchain(SystemApi::Window *w,AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx   = reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VSwapchain(*dx,w);
  }

AbstractGraphicsApi::PPipeline VulkanApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                         const RenderState &st, size_t stride, Topology tp,
                                                         const PipelineLay& ulayImpl,
                                                         const Shader* vs, const Shader* tc, const Shader* te,
                                                         const Shader* gs, const Shader* fs) {
  auto* dx   = reinterpret_cast<Detail::VDevice*>(d);
  auto* vert = reinterpret_cast<const Detail::VShader*>(vs);
  auto* ctrl = reinterpret_cast<const Detail::VShader*>(tc);
  auto* tess = reinterpret_cast<const Detail::VShader*>(te);
  auto* geom = reinterpret_cast<const Detail::VShader*>(gs);
  auto* frag = reinterpret_cast<const Detail::VShader*>(fs);
  auto& ul   = reinterpret_cast<const Detail::VPipelineLay&>(ulayImpl);

  return PPipeline(new Detail::VPipeline(*dx,st,stride,tp,ul,vert,ctrl,tess,geom,frag));
  }

AbstractGraphicsApi::PCompPipeline VulkanApi::createComputePipeline(AbstractGraphicsApi::Device* d,
                                                                const AbstractGraphicsApi::PipelineLay& ulayImpl,
                                                                AbstractGraphicsApi::Shader* shader) {
  auto*   dx = reinterpret_cast<Detail::VDevice*>(d);
  auto&   ul = reinterpret_cast<const Detail::VPipelineLay&>(ulayImpl);

  return PCompPipeline(new Detail::VCompPipeline(*dx,ul,*reinterpret_cast<Detail::VShader*>(shader)));
  }

AbstractGraphicsApi::PShader VulkanApi::createShader(AbstractGraphicsApi::Device *d, const void* source, size_t src_size) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return PShader(new Detail::VShader(*dx,source,src_size));
  }

AbstractGraphicsApi::Fence *VulkanApi::createFence(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx =reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VFence(*dx);
  }

AbstractGraphicsApi::PBuffer VulkanApi::createBuffer(AbstractGraphicsApi::Device *d,
                                                     const void *mem, size_t count, size_t size, size_t alignedSz,
                                                     MemUsage usage, BufferHeap flg) {
  Detail::VDevice& dx = *reinterpret_cast<Detail::VDevice*>(d);

  if(flg==BufferHeap::Upload) {
    VBuffer buf = dx.allocator.alloc(mem,count,size,alignedSz,usage|MemUsage::TransferSrc,BufferHeap::Upload);
    return PBuffer(new VBuffer(std::move(buf)));
    }

  if(flg==BufferHeap::Readback) {
    VBuffer buf = dx.allocator.alloc(mem,count,size,alignedSz,usage|MemUsage::TransferDst,BufferHeap::Readback);
    return PBuffer(new VBuffer(std::move(buf)));
    }

  VBuffer buf = dx.allocator.alloc(nullptr,count,size,alignedSz, usage|MemUsage::TransferDst|MemUsage::TransferSrc,BufferHeap::Device);
  if(mem==nullptr)
    return PBuffer(new VBuffer(std::move(buf)));

  DSharedPtr<Buffer*> pbuf(new VBuffer(std::move(buf)));
  pbuf.handler->update(mem,0,count,size,alignedSz);
  return PBuffer(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createTexture(AbstractGraphicsApi::Device *d, const Pixmap &p, TextureFormat frm, uint32_t mipCnt) {
  Detail::VDevice& dx     = *reinterpret_cast<Detail::VDevice*>(d);

  const uint32_t   size   = uint32_t(p.dataSize());
  VkFormat         format = Detail::nativeFormat(frm);
  Pixmap::Format   pfrm   = Pixmap::toPixmapFormat(frm);

  Detail::VBuffer  stage  = dx.allocator.alloc(p.data(),size,1,1,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::VTexture buf    = dx.allocator.alloc(p,mipCnt,format);

  Detail::DSharedPtr<Buffer*>  pstage(new Detail::VBuffer (std::move(stage)));
  Detail::DSharedPtr<Texture*> pbuf  (new Detail::VTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pstage);
  cmd->hold(pbuf);

  if(isCompressedFormat(frm)) {
    cmd->barrier(*pbuf.handler, ResourceAccess::None, ResourceAccess::TransferDst, uint32_t(-1));
    size_t blockSize  = Pixmap::blockSizeForFormat(pfrm);
    size_t bufferSize = 0;

    uint32_t w = uint32_t(p.w()), h = uint32_t(p.h());
    for(uint32_t i=0; i<mipCnt; i++){
      cmd->copy(*pbuf.handler,w,h,i,*pstage.handler,bufferSize);

      Size bsz   = Pixmap::blockCount(pfrm,w,h);
      bufferSize += bsz.w*bsz.h*blockSize;
      w = std::max<uint32_t>(1,w/2);
      h = std::max<uint32_t>(1,h/2);
      }

    cmd->barrier(*pbuf.handler, ResourceAccess::TransferDst, ResourceAccess::Sampler, uint32_t(-1));
    } else {
    cmd->barrier(*pbuf.handler, ResourceAccess::None, ResourceAccess::TransferDst, uint32_t(-1));
    cmd->copy(*pbuf.handler,p.w(),p.h(),0,*pstage.handler,0);
    cmd->barrier(*pbuf.handler, ResourceAccess::TransferDst, ResourceAccess::Sampler, uint32_t(-1));
    if(mipCnt>1)
      cmd->generateMipmap(*pbuf.handler, p.w(), p.h(), mipCnt);
    }
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createTexture(AbstractGraphicsApi::Device *d,
                                                       const uint32_t w, const uint32_t h, uint32_t mipCnt,
                                                       TextureFormat frm) {
  Detail::VDevice* dx = reinterpret_cast<Detail::VDevice*>(d);
  
  Detail::VTexture buf=dx->allocator.alloc(w,h,mipCnt,frm,false);
  Detail::DSharedPtr<Detail::VTexture*> pbuf(new Detail::VTextureWithFbo(std::move(buf)));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createStorage(AbstractGraphicsApi::Device* d,
                                                       const uint32_t w, const uint32_t h, uint32_t mipCnt,
                                                       TextureFormat frm) {
  Detail::VDevice& dx = *reinterpret_cast<Detail::VDevice*>(d);

  Detail::VTexture buf=dx.allocator.alloc(w,h,mipCnt,frm,true);
  Detail::DSharedPtr<Texture*> pbuf(new Detail::VTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->barrier(*pbuf.handler,ResourceAccess::None,ResourceAccess::Unordered,uint32_t(-1));
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::AccelerationStructure* VulkanApi::createBottomAccelerationStruct(Device* d,
                                                                                      Buffer* vbo, size_t stride,
                                                                                      Buffer* ibo, Detail::IndexClass icls) {
  Detail::VDevice& dx = *reinterpret_cast<Detail::VDevice*>(d);
  auto&            vx = *reinterpret_cast<VBuffer*>(vbo);
  auto&            ix = *reinterpret_cast<VBuffer*>(ibo);
  return new VAccelerationStructure(dx,vx,stride,ix,icls);
  }

void VulkanApi::readPixels(AbstractGraphicsApi::Device *d, Pixmap& out, const PTexture t,
                           TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) {
  Detail::VDevice&  dx = *reinterpret_cast<Detail::VDevice*>(d);
  Detail::VTexture& tx = *reinterpret_cast<Detail::VTexture*>(t.handler);

  Pixmap::Format  pfrm   = Pixmap::toPixmapFormat(frm);
  size_t          bpb    = Pixmap::blockSizeForFormat(pfrm);
  Size            bsz    = Pixmap::blockCount(pfrm,w,h);

  const size_t    size   = bsz.w*bsz.h*bpb;
  Detail::VBuffer stage  = dx.allocator.alloc(nullptr,size,1,1,MemUsage::TransferDst,BufferHeap::Readback);
  ResourceAccess  defLay = storageImg ? ResourceAccess::ComputeRead : ResourceAccess::Sampler;

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->barrier(tx,defLay,ResourceAccess::TransferSrc,uint32_t(-1));
  cmd->copyNative(stage,0, tx,w,h,mip);
  cmd->barrier(tx,ResourceAccess::TransferSrc,defLay,uint32_t(-1));
  cmd->end();

  dx.dataMgr().waitFor(&tx);
  dx.dataMgr().submitAndWait(std::move(cmd));

  out = Pixmap(w,h,pfrm);
  stage.read(out.data(),0,size);
  }

void VulkanApi::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer* buf, void* out, size_t size) {
  Detail::VBuffer&  bx = *reinterpret_cast<Detail::VBuffer*>(buf);
  bx.read(out,0,size);
  }

AbstractGraphicsApi::Desc* VulkanApi::createDescriptors(AbstractGraphicsApi::Device* d, PipelineLay& ulayImpl) {
  auto* dx = reinterpret_cast<Detail::VDevice*>(d);
  auto& ul = reinterpret_cast<Detail::VPipelineLay&>(ulayImpl);
  return new Detail::VDescriptorArray(dx->device.impl,ul);
  }

AbstractGraphicsApi::PPipelineLay VulkanApi::createPipelineLayout(Device *d,
                                                                  const Shader* vs, const Shader* tc, const Shader* te,
                                                                  const Shader* gs, const Shader* fs, const Shader* cs) {
  auto* dx = reinterpret_cast<Detail::VDevice*>(d);
  if(cs!=nullptr) {
    auto* comp = reinterpret_cast<const Detail::VShader*>(cs);
    return PPipelineLay(new Detail::VPipelineLay(*dx,comp->lay));
    }

  const Shader* sh[] = {vs,tc,te,gs,fs};
  const std::vector<Detail::ShaderReflection::Binding>* lay[5] = {};
  for(size_t i=0; i<5; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto* s = reinterpret_cast<const Detail::VShader*>(sh[i]);
    lay[i] = &s->lay;
    }
  return PPipelineLay(new Detail::VPipelineLay(*dx,lay,5));
  }

AbstractGraphicsApi::CommandBuffer* VulkanApi::createCommandBuffer(AbstractGraphicsApi::Device* d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VCommandBuffer(*dx);
  }

void VulkanApi::present(Device*, Swapchain *sw) {
  Detail::VSwapchain* sx=reinterpret_cast<Detail::VSwapchain*>(sw);
  sx->present();
  }

void VulkanApi::submit(Device *d, CommandBuffer* cmd, Fence *doneCpu) {
  Detail::VDevice*        dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VCommandBuffer* cx=reinterpret_cast<Detail::VCommandBuffer*>(cmd);
  auto*                   rc=reinterpret_cast<Detail::VFence*>(doneCpu);

  impl->submit(dx,&cx,1,rc);
  }

void VulkanApi::submit(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::CommandBuffer **cmd, size_t count, Fence* doneCpu) {
  auto* dx = reinterpret_cast<VDevice*>(d);
  auto* rc = reinterpret_cast<VFence*>(doneCpu);
  impl->submit(dx,reinterpret_cast<VCommandBuffer**>(cmd),count,rc);
  }

void VulkanApi::getCaps(Device *d, Props& props) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  props=dx->props;
  }

#endif
