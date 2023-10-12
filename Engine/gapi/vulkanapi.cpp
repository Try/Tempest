#include "vulkanapi.h"

#if defined(TEMPEST_BUILD_VULKAN)

#include "vulkan/vdevice.h"
#include "vulkan/vswapchain.h"
#include "vulkan/vpipeline.h"
#include "vulkan/vbuffer.h"
#include "vulkan/vshader.h"
#include "vulkan/vfence.h"
#include "vulkan/vmeshshaderemulated.h"
#include "vulkan/vcommandbuffer.h"
#include "vulkan/vdescriptorarray.h"
#include "vulkan/vpipelinelay.h"
#include "vulkan/vtexture.h"
#include "vulkan/vaccelerationstructure.h"

#include "shaderreflection.h"

#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <Tempest/PipelineLayout>
#include <Tempest/Application>

#include <libspirv/libspirv.h>

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

AbstractGraphicsApi::Device *VulkanApi::createDevice(std::string_view gpuName) {
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

AbstractGraphicsApi::PPipelineLay VulkanApi::createPipelineLayout(Device* d, const Shader*const* sh, size_t count) {
  const std::vector<Detail::ShaderReflection::Binding>* lay[5] = {};
  for(size_t i=0; i<count; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto* s = reinterpret_cast<const Detail::VShader*>(sh[i]);
    lay[i] = &s->lay;
    }
  auto& dx = *reinterpret_cast<Detail::VDevice*>(d);
  return PPipelineLay(new Detail::VPipelineLay(dx,lay,count));
  }

AbstractGraphicsApi::PPipeline VulkanApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                         const RenderState &st, Topology tp, const PipelineLay& ulayImpl,
                                                         const Shader*const* sh, size_t cnt) {
  auto* dx   = reinterpret_cast<Detail::VDevice*>(d);
  auto& ul   = reinterpret_cast<const Detail::VPipelineLay&>(ulayImpl);
  const Detail::VShader* shader[5] = {};
  for(size_t i=0; i<cnt; ++i)
    shader[i] = reinterpret_cast<const Detail::VShader*>(sh[i]);
  return PPipeline(new Detail::VPipeline(*dx,st,tp,ul,shader,cnt));
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
  if(dx->props.meshlets.meshShaderEmulated) {
    libspirv::Bytecode code(reinterpret_cast<const uint32_t*>(source),src_size/4);
    if(code.findExecutionModel()==spv::ExecutionModelMeshEXT) {
      return PShader(new Detail::VMeshShaderEmulated(*dx,source,src_size));
      }
    }
  return PShader(new Detail::VShader(*dx,source,src_size));
  }

AbstractGraphicsApi::Fence *VulkanApi::createFence(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx =reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VFence(*dx);
  }

AbstractGraphicsApi::PBuffer VulkanApi::createBuffer(AbstractGraphicsApi::Device *d, const void *mem, size_t size,
                                                     MemUsage usage, BufferHeap flg) {
  Detail::VDevice& dx = *reinterpret_cast<Detail::VDevice*>(d);

  if(flg==BufferHeap::Upload) {
    VBuffer buf = dx.allocator.alloc(mem, size, usage|MemUsage::TransferSrc, BufferHeap::Upload);
    return PBuffer(new VBuffer(std::move(buf)));
    }

  if(flg==BufferHeap::Readback) {
    VBuffer buf = dx.allocator.alloc(mem, size, usage|MemUsage::TransferDst, BufferHeap::Readback);
    return PBuffer(new VBuffer(std::move(buf)));
    }

  VBuffer buf = dx.allocator.alloc(nullptr, size, usage|MemUsage::TransferDst|MemUsage::TransferSrc, BufferHeap::Device);
  if(mem==nullptr)
    return PBuffer(new VBuffer(std::move(buf)));

  DSharedPtr<Buffer*> pbuf(new VBuffer(std::move(buf)));
  pbuf.handler->update(mem,0,size);
  return PBuffer(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createTexture(AbstractGraphicsApi::Device *d, const Pixmap &p, TextureFormat frm, uint32_t mipCnt) {
  Detail::VDevice& dx     = *reinterpret_cast<Detail::VDevice*>(d);

  const uint32_t   size   = uint32_t(p.dataSize());
  VkFormat         format = Detail::nativeFormat(frm);

  Detail::VBuffer  stage  = dx.allocator.alloc(p.data(),size,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::VTexture buf    = dx.allocator.alloc(p,mipCnt,format);

  Detail::DSharedPtr<Buffer*>  pstage(new Detail::VBuffer (std::move(stage)));
  Detail::DSharedPtr<Texture*> pbuf  (new Detail::VTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pstage);
  cmd->hold(pbuf);

  if(isCompressedFormat(frm)) {
    cmd->barrier(*pbuf.handler, ResourceAccess::None, ResourceAccess::TransferDst, uint32_t(-1));
    size_t blockSize  = Pixmap::blockSizeForFormat(frm);
    size_t bufferSize = 0;

    uint32_t w = uint32_t(p.w()), h = uint32_t(p.h());
    for(uint32_t i=0; i<mipCnt; i++){
      cmd->copy(*pbuf.handler,w,h,i,*pstage.handler,bufferSize);

      Size bsz   = Pixmap::blockCount(frm,w,h);
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
  
  Detail::VTexture buf=dx->allocator.alloc(w,h,1,mipCnt,frm,false);
  Detail::DSharedPtr<Detail::VTexture*> pbuf(new Detail::VTextureWithFbo(std::move(buf)));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createStorage(Device* d,
                                                       const uint32_t w, const uint32_t h, uint32_t mipCnt,
                                                       TextureFormat frm) {
  Detail::VDevice& dx = *reinterpret_cast<Detail::VDevice*>(d);

  Detail::VTexture buf=dx.allocator.alloc(w,h,1,mipCnt,frm,true);
  Detail::DSharedPtr<Texture*> pbuf(new Detail::VTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->barrier(*pbuf.handler,ResourceAccess::None,ResourceAccess::UavReadWriteAll,uint32_t(-1));
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createStorage(Device* d,
                                                       const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mipCnt,
                                                       TextureFormat frm) {
  Detail::VDevice& dx = *reinterpret_cast<Detail::VDevice*>(d);

  Detail::VTexture buf=dx.allocator.alloc(w,h,depth,mipCnt,frm,true);
  Detail::DSharedPtr<Texture*> pbuf(new Detail::VTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->barrier(*pbuf.handler,ResourceAccess::None,ResourceAccess::UavReadWriteAll,uint32_t(-1));
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::AccelerationStructure* VulkanApi::createBottomAccelerationStruct(Device* d, const RtGeometry* geom, size_t size) {
  auto& dx = *reinterpret_cast<VDevice*>(d);
  return new VAccelerationStructure(dx, geom, size);
  }

AbstractGraphicsApi::AccelerationStructure* VulkanApi::createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t size) {
  auto& dx = *reinterpret_cast<VDevice*>(d);
  return new VTopAccelerationStructure(dx, inst, as, size);
  }

void VulkanApi::readPixels(AbstractGraphicsApi::Device *d, Pixmap& out, const PTexture t,
                           TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) {
  auto&           dx     = *reinterpret_cast<VDevice*>(d);
  auto&           tx     = *reinterpret_cast<VTexture*>(t.handler);

  size_t          bpb    = Pixmap::blockSizeForFormat(frm);
  Size            bsz    = Pixmap::blockCount(frm,w,h);

  const size_t    size   = bsz.w*bsz.h*bpb;
  Detail::VBuffer stage  = dx.allocator.alloc(nullptr, size, MemUsage::TransferDst, BufferHeap::Readback);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  if(storageImg) {
    cmd->copyNative(stage,0, tx,w,h,mip);
    }
  else if(isDepthFormat(frm)) {
    cmd->barrier(tx,ResourceAccess::DepthReadOnly,ResourceAccess::TransferSrc,uint32_t(-1));
    cmd->copyNative(stage,0, tx,w,h,mip);
    cmd->barrier(tx,ResourceAccess::TransferSrc,ResourceAccess::DepthReadOnly,uint32_t(-1));
    }
  else {
    cmd->barrier(tx,ResourceAccess::Sampler,ResourceAccess::TransferSrc,uint32_t(-1));
    cmd->copyNative(stage,0, tx,w,h,mip);
    cmd->barrier(tx,ResourceAccess::TransferSrc,ResourceAccess::Sampler,uint32_t(-1));
    }
  cmd->end();

  dx.dataMgr().waitFor(&tx);
  dx.dataMgr().submitAndWait(std::move(cmd));

  out = Pixmap(w,h,frm);
  stage.read(out.data(),0,size);
  }

void VulkanApi::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer* buf, void* out, size_t size) {
  Detail::VBuffer&  bx = *reinterpret_cast<Detail::VBuffer*>(buf);
  bx.read(out,0,size);
  }

AbstractGraphicsApi::Desc* VulkanApi::createDescriptors(AbstractGraphicsApi::Device* d, PipelineLay& ulayImpl) {
  auto& dx = *reinterpret_cast<Detail::VDevice*>(d);
  auto& ul = reinterpret_cast<Detail::VPipelineLay&>(ulayImpl);
  return new Detail::VDescriptorArray(dx,ul);
  }

AbstractGraphicsApi::CommandBuffer* VulkanApi::createCommandBuffer(AbstractGraphicsApi::Device* d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  if(dx->props.meshlets.meshShaderEmulated) {
    // CB with meshlet emulation support
    return new Detail::VMeshCommandBuffer(*dx);
    }
  return new Detail::VCommandBuffer(*dx);
  }

void VulkanApi::present(Device*, Swapchain *sw) {
  Detail::VSwapchain* sx=reinterpret_cast<Detail::VSwapchain*>(sw);
  sx->present();
  }

void VulkanApi::submit(Device *d, CommandBuffer* cmd, Fence *sync) {
  Detail::VDevice&        dx    = *reinterpret_cast<Detail::VDevice*>(d);
  Detail::VCommandBuffer& cx    = *reinterpret_cast<Detail::VCommandBuffer*>(cmd);
  auto*                   fence =  reinterpret_cast<Detail::VFence*>(sync);

  dx.dataMgr().wait();
  dx.submit(cx,fence);
  }

void VulkanApi::getCaps(Device *d, Props& props) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  props=dx->props;
  }

#endif
