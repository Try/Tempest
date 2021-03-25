#include "vulkanapi.h"

#include "vulkan/vdevice.h"
#include "vulkan/vswapchain.h"
#include "vulkan/vrenderpass.h"
#include "vulkan/vpipeline.h"
#include "vulkan/vframebuffer.h"
#include "vulkan/vframebufferlayout.h"
#include "vulkan/vbuffer.h"
#include "vulkan/vshader.h"
#include "vulkan/vfence.h"
#include "vulkan/vsemaphore.h"
#include "vulkan/vcommandpool.h"
#include "vulkan/vcommandbuffer.h"
#include "vulkan/vdescriptorarray.h"
#include "vulkan/vuniformslay.h"
#include "vulkan/vtexture.h"
#include "vulkan/vuniformslay.h"

#include "deviceallocator.h"

#include "vulkan/vulkan_sdk.h"

#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <Tempest/UniformsLayout>
#include <Tempest/Application>

using namespace Tempest;

struct VulkanApi::Impl : public Detail::VulkanApi {
  using VulkanApi::VulkanApi;
  std::mutex                        syncBuf;
  std::vector<VkCommandBuffer>      cmdBuf;
  std::vector<VkSemaphore>          semBuf;
  std::vector<VkPipelineStageFlags> semFlgBuf;
  };

VulkanApi::VulkanApi(ApiFlags f) {
  impl.reset(new Impl(bool(f&ApiFlags::Validation)));
  }

VulkanApi::~VulkanApi(){
  }

std::vector<AbstractGraphicsApi::Props> VulkanApi::devices() const {
  return impl->devices();
  }

AbstractGraphicsApi::Device *VulkanApi::createDevice(const char* gpuName) {
  return new Detail::VDevice(*impl,gpuName);
  }

void VulkanApi::destroy(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  delete dx;
  }

AbstractGraphicsApi::Swapchain *VulkanApi::createSwapchain(SystemApi::Window *w,AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx   = reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VSwapchain(*dx,w);
  }

AbstractGraphicsApi::PPass VulkanApi::createPass(AbstractGraphicsApi::Device *d,
                                                 const FboMode** att,
                                                 size_t acount) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return PPass(new Detail::VRenderPass(*dx,att,uint8_t(acount)));
  }

AbstractGraphicsApi::PFbo VulkanApi::createFbo(AbstractGraphicsApi::Device *d, FboLayout* lay,
                                               uint32_t w, uint32_t h, uint8_t clCount,
                                               Swapchain** s, Texture** cl, const uint32_t* imgId,
                                               AbstractGraphicsApi::Texture *zbuf) {
  Detail::VDevice*            dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VFramebufferLayout* l =reinterpret_cast<Detail::VFramebufferLayout*>(lay);
  Detail::VTexture*           zb=reinterpret_cast<Detail::VTexture*>(zbuf);

  Detail::VTexture*   att[256] = {};
  Detail::VSwapchain* sw[256] = {};
  for(size_t i=0; i<clCount; ++i) {
    att[i] = reinterpret_cast<Detail::VTexture*>  (cl[i]);
    sw [i] = reinterpret_cast<Detail::VSwapchain*>(s[i]);
    }
  return PFbo(new Detail::VFramebuffer(*dx,*l, w,h,clCount, sw,att,imgId, zb));
  }

AbstractGraphicsApi::PFboLayout VulkanApi::createFboLayout(AbstractGraphicsApi::Device *d,
                                                           Swapchain** s,
                                                           Tempest::TextureFormat *att,
                                                           uint8_t attCount) {
  Detail::VDevice&    dx=*reinterpret_cast<Detail::VDevice*>(d);
  VkFormat            frm[256] = {};
  Detail::VSwapchain* sx[256] = {};

  for(size_t i=0;i<attCount;++i){
    frm[i] = Detail::nativeFormat(att[i]);
    sx[i]  = reinterpret_cast<Detail::VSwapchain*>(s[i]);
    }

  Detail::DSharedPtr<AbstractGraphicsApi::FboLayout*> impl{
    new Detail::VFramebufferLayout(dx,sx,frm,uint8_t(attCount))
    };

  return impl;
  }

AbstractGraphicsApi::PPipeline VulkanApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                         const RenderState &st, size_t stride, Topology tp,
                                                         const UniformsLay& ulayImpl,
                                                         const Shader* vs, const Shader* tc, const Shader* te,
                                                         const Shader* gs, const Shader* fs) {
  auto* dx   = reinterpret_cast<Detail::VDevice*>(d);
  auto* vert = reinterpret_cast<const Detail::VShader*>(vs);
  auto* ctrl = reinterpret_cast<const Detail::VShader*>(tc);
  auto* tess = reinterpret_cast<const Detail::VShader*>(te);
  auto* geom = reinterpret_cast<const Detail::VShader*>(gs);
  auto* frag = reinterpret_cast<const Detail::VShader*>(fs);
  auto& ul   = reinterpret_cast<const Detail::VUniformsLay&>(ulayImpl);

  return PPipeline(new Detail::VPipeline(*dx,st,stride,tp,ul,vert,ctrl,tess,geom,frag));
  }

AbstractGraphicsApi::PCompPipeline VulkanApi::createComputePipeline(AbstractGraphicsApi::Device* d,
                                                                const AbstractGraphicsApi::UniformsLay& ulayImpl,
                                                                AbstractGraphicsApi::Shader* shader) {
  auto*   dx = reinterpret_cast<Detail::VDevice*>(d);
  auto&   ul = reinterpret_cast<const Detail::VUniformsLay&>(ulayImpl);

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

AbstractGraphicsApi::Semaphore *VulkanApi::createSemaphore(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VSemaphore(*dx);
  }

AbstractGraphicsApi::PBuffer VulkanApi::createBuffer(AbstractGraphicsApi::Device *d,
                                                     const void *mem, size_t count, size_t size, size_t alignedSz,
                                                     MemUsage usage, BufferHeap flg) {
  Detail::VDevice& dx = *reinterpret_cast<Detail::VDevice*>(d);

  if(flg==BufferHeap::Upload) {
    Detail::VBuffer stage=dx.allocator.alloc(mem,count,size,alignedSz,usage|MemUsage::TransferSrc,BufferHeap::Upload);
    return PBuffer(new Detail::VBuffer(std::move(stage)));
    }

  Detail::VBuffer buf = dx.allocator.alloc(nullptr, count,size,alignedSz, usage|MemUsage::TransferDst|MemUsage::TransferSrc,BufferHeap::Device);
  if(mem==nullptr) {
    Detail::DSharedPtr<Buffer*> pbuf(new Detail::VBuffer(std::move(buf)));
    return PBuffer(pbuf.handler);
    }

  Detail::DSharedPtr<Buffer*> pbuf(new Detail::VBuffer(std::move(buf)));
  pbuf.handler->update(mem,0,count,size,alignedSz);
  return PBuffer(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createTexture(AbstractGraphicsApi::Device *d, const Pixmap &p, TextureFormat frm, uint32_t mipCnt) {
  Detail::VDevice& dx     = *reinterpret_cast<Detail::VDevice*>(d);
  const uint32_t   size   = uint32_t(p.dataSize());
  VkFormat         format = Detail::nativeFormat(frm);
  Detail::VBuffer  stage  = dx.allocator.alloc(p.data(),size,1,1,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::VTexture buf    = dx.allocator.alloc(p,mipCnt,format);

  Detail::DSharedPtr<Buffer*>  pstage(new Detail::VBuffer (std::move(stage)));
  Detail::DSharedPtr<Texture*> pbuf  (new Detail::VTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pstage);
  cmd->hold(pbuf);

  cmd->changeLayout(*pbuf.handler, TextureLayout::Undefined, TextureLayout::TransferDest, uint32_t(-1));
  if(isCompressedFormat(frm)){
    size_t blocksize  = (frm==TextureFormat::DXT1) ? 8 : 16;
    size_t bufferSize = 0;

    uint32_t w = uint32_t(p.w()), h = uint32_t(p.h());
    for(uint32_t i=0; i<mipCnt; i++){
      size_t blockcount = ((w+3)/4)*((h+3)/4);
      cmd->copy(*pbuf.handler,w,h,i,*pstage.handler,bufferSize);

      bufferSize += blockcount*blocksize;
      w = std::max<uint32_t>(1,w/2);
      h = std::max<uint32_t>(1,h/2);
      }

    cmd->changeLayout(*pbuf.handler, TextureLayout::TransferDest, TextureLayout::Sampler, uint32_t(-1));
    } else {
    cmd->copy(*pbuf.handler,p.w(),p.h(),0,*pstage.handler,0);
    if(mipCnt>1)
      cmd->generateMipmap(*pbuf.handler, TextureLayout::TransferDest, p.w(), p.h(), mipCnt); else
      cmd->changeLayout(*pbuf.handler, TextureLayout::TransferDest, TextureLayout::Sampler, uint32_t(-1));
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
  Detail::DSharedPtr<Detail::VTexture*> pbuf(new Detail::VTexture(std::move(buf)));

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
  cmd->changeLayout(*pbuf.handler,TextureLayout::Undefined,TextureLayout::Unordered,uint32_t(-1));
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));

  return PTexture(pbuf.handler);
  }

void VulkanApi::readPixels(AbstractGraphicsApi::Device *d, Pixmap& out, const PTexture t,
                           TextureLayout lay, TextureFormat frm,
                           const uint32_t w, const uint32_t h, uint32_t mip) {
  Detail::VDevice&  dx = *reinterpret_cast<Detail::VDevice*>(d);
  Detail::VTexture& tx = *reinterpret_cast<Detail::VTexture*>(t.handler);

  Pixmap::Format  pfrm  = Pixmap::toPixmapFormat(frm);
  size_t          bpp   = Pixmap::bppForFormat(pfrm);
  if(bpp==0)
    throw std::runtime_error("not implemented");

  const size_t    size  = w*h*bpp;
  Detail::VBuffer stage = dx.allocator.alloc(nullptr,size,1,1,MemUsage::TransferDst,BufferHeap::Readback);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->changeLayout(tx, lay, TextureLayout::TransferSrc, mip);
  cmd->copy(stage,w,h,mip,tx,0);
  cmd->changeLayout(tx, TextureLayout::TransferSrc, lay, mip);
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

AbstractGraphicsApi::Desc *VulkanApi::createDescriptors(AbstractGraphicsApi::Device* d, UniformsLay& ulayImpl) {
  auto* dx = reinterpret_cast<Detail::VDevice*>(d);
  auto& ul = reinterpret_cast<Detail::VUniformsLay&>(ulayImpl);
  if(ul.lay.size()==0)
    return nullptr;
  return new Detail::VDescriptorArray(dx->device,ul);
  }

AbstractGraphicsApi::PUniformsLay VulkanApi::createUboLayout(Device *d,
                                                             const Shader* vs, const Shader* tc, const Shader* te,
                                                             const Shader* gs, const Shader* fs, const Shader* cs) {
  auto* dx = reinterpret_cast<Detail::VDevice*>(d);
  if(cs!=nullptr) {
    auto* comp = reinterpret_cast<const Detail::VShader*>(cs);
    return PUniformsLay(new Detail::VUniformsLay(*dx,comp->lay));
    }

  const Shader* sh[] = {vs,tc,te,gs,fs};
  const std::vector<UniformsLayout::Binding>* lay[5] = {};
  for(size_t i=0; i<5; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto* s = reinterpret_cast<const Detail::VShader*>(sh[i]);
    lay[i] = &s->lay;
    }
  return PUniformsLay(new Detail::VUniformsLay(*dx,lay,5));
  }

AbstractGraphicsApi::CommandBuffer* VulkanApi::createCommandBuffer(AbstractGraphicsApi::Device* d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VCommandBuffer(*dx);
  }

void VulkanApi::present(Device *d,Swapchain *sw,uint32_t imageId,const Semaphore *wait) {
  Detail::VDevice*    dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VSwapchain* sx=reinterpret_cast<Detail::VSwapchain*>(sw);
  auto*               wx=reinterpret_cast<const Detail::VSemaphore*>(wait);

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = &wx->impl;

  VkSwapchainKHR swapChains[] = {sx->swapChain};
  presentInfo.swapchainCount  = 1;
  presentInfo.pSwapchains     = swapChains;
  presentInfo.pImageIndices   = &imageId;

  dx->dataMgr().wait();

  //auto t = Application::tickCount();
  VkResult code = dx->presentQueue->present(presentInfo);
  if(code==VK_ERROR_OUT_OF_DATE_KHR || code==VK_SUBOPTIMAL_KHR)
    throw SwapchainSuboptimal();
  //Log::i("vkQueuePresentKHR = ",Application::tickCount()-t);
  Detail::vkAssert(code);
  }

void VulkanApi::submit(Device *d,
                       CommandBuffer *cmd,
                       Semaphore *wait,
                       Semaphore *onReady,
                       Fence *onReadyCpu) {
  Detail::VDevice*        dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VCommandBuffer* cx=reinterpret_cast<Detail::VCommandBuffer*>(cmd);
  auto*                   wx=reinterpret_cast<const Detail::VSemaphore*>(wait);
  auto*                   rx=reinterpret_cast<Detail::VSemaphore*>(onReady);
  auto*                   rc=reinterpret_cast<const Detail::VFence*>(onReadyCpu);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore          waitSemaphores[] = {wx->impl};
  VkPipelineStageFlags waitStages[]     = {wx->stage};

  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores    = waitSemaphores;
  submitInfo.pWaitDstStageMask  = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cx->impl;

  if(rx!=nullptr) {
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &rx->impl;
    rx->stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

  if(onReadyCpu!=nullptr)
    onReadyCpu->reset();

  dx->dataMgr().wait();
  dx->graphicsQueue->submit(1,&submitInfo,rc==nullptr ? VK_NULL_HANDLE : rc->impl);
  }

void VulkanApi::submit(AbstractGraphicsApi::Device *d,
                       AbstractGraphicsApi::CommandBuffer **cmd, size_t count,
                       Semaphore **wait, size_t waitCnt,
                       Semaphore **done, size_t doneCnt,
                       Fence     *doneCpu) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);

  std::lock_guard<std::mutex> guard(impl->syncBuf);
  auto& cmdBuf    = impl->cmdBuf;
  auto& semBuf    = impl->semBuf;
  auto& semFlgBuf = impl->semFlgBuf;

  cmdBuf.resize(count);
  for(size_t i=0;i<count;++i) {
    auto* cx=reinterpret_cast<Detail::VCommandBuffer*>(cmd[i]);
    cmdBuf[i] = cx->impl;
    }

  semBuf.resize(waitCnt+doneCnt);
  semFlgBuf.resize(waitCnt);
  for(size_t i=0;i<waitCnt;++i) {
    auto* sx=reinterpret_cast<Detail::VSemaphore*>(wait[i]);
    semBuf[i]    = sx->impl;
    semFlgBuf[i] = sx->stage;
    }
  for(size_t i=0;i<doneCnt;++i) {
    auto* sx=reinterpret_cast<Detail::VSemaphore*>(done[i]);
    semBuf[i+waitCnt] = sx->impl;
    sx->stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  submitInfo.waitSemaphoreCount   = uint32_t(waitCnt);
  submitInfo.pWaitSemaphores      = semBuf.data();
  submitInfo.pWaitDstStageMask    = semFlgBuf.data();

  submitInfo.signalSemaphoreCount = uint32_t(doneCnt);
  submitInfo.pSignalSemaphores    = semBuf.data()+waitCnt;

  submitInfo.commandBufferCount   = uint32_t(count);
  submitInfo.pCommandBuffers      = cmdBuf.data();

  if(doneCpu!=nullptr)
    doneCpu->reset();
  auto* rc=reinterpret_cast<Detail::VFence*>(doneCpu);

  dx->dataMgr().wait();
  dx->graphicsQueue->submit(1,&submitInfo,rc==nullptr ? VK_NULL_HANDLE : rc->impl);
  }

void VulkanApi::getCaps(Device *d, Props& props) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  props=dx->props;
  }

