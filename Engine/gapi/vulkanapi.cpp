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
#include "vulkan/vtexture.h"

#include "deviceallocator.h"

#include <vulkan/vulkan.hpp>

#include <Tempest/Pixmap>
#include <Tempest/Log>

using namespace Tempest;

struct VulkanApi::Impl : public Detail::VulkanApi {
  using VulkanApi::VulkanApi;
  std::mutex                   syncBuf;
  std::vector<VkCommandBuffer> cmdBuf;
  std::vector<VkSemaphore>     semBuf;
  };

VulkanApi::VulkanApi(ApiFlags f) {
  impl.reset(new Impl(bool(f&ApiFlags::Validation)));
  }

VulkanApi::~VulkanApi(){
  }

AbstractGraphicsApi::Device *VulkanApi::createDevice(SystemApi::Window *w) {
  return new Detail::VDevice(*impl,w);
  }

void VulkanApi::destroy(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  delete dx;
  }

void VulkanApi::waitIdle(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  vkDeviceWaitIdle(dx->device);
  }

AbstractGraphicsApi::Swapchain *VulkanApi::createSwapchain(SystemApi::Window *w,AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  const uint32_t width =SystemApi::width(w);
  const uint32_t height=SystemApi::height(w);
  return new Detail::VSwapchain(*dx,width,height);
  }

AbstractGraphicsApi::PPass VulkanApi::createPass(AbstractGraphicsApi::Device *d,
                                                 const Attachment** att,
                                                 size_t acount) {
  Detail::VDevice*    dx=reinterpret_cast<Detail::VDevice*>(d);
  return PPass(new Detail::VRenderPass(*dx,att,uint8_t(acount)));
  }

AbstractGraphicsApi::PFbo VulkanApi::createFbo(AbstractGraphicsApi::Device *d,
                                               FboLayout* lay,
                                               AbstractGraphicsApi::Swapchain *s,
                                               uint32_t imageId) {
  Detail::VDevice*            dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VFramebufferLayout* l =reinterpret_cast<Detail::VFramebufferLayout*>(lay);
  Detail::VSwapchain*         sx=reinterpret_cast<Detail::VSwapchain*>(s);

  return PFbo(new Detail::VFramebuffer(*dx,*l,*sx,imageId));
  }

AbstractGraphicsApi::PFbo VulkanApi::createFbo(AbstractGraphicsApi::Device *d,
                                               FboLayout* lay,
                                               AbstractGraphicsApi::Swapchain *s,
                                               uint32_t imageId,
                                               AbstractGraphicsApi::Texture *zbuf) {
  Detail::VDevice*            dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VFramebufferLayout* l =reinterpret_cast<Detail::VFramebufferLayout*>(lay);
  Detail::VSwapchain*         sx=reinterpret_cast<Detail::VSwapchain*>(s);
  Detail::VTexture*           tx=reinterpret_cast<Detail::VTexture*>(zbuf);

  return PFbo(new Detail::VFramebuffer(*dx,*l,*sx,imageId,*tx));
  }

AbstractGraphicsApi::PFbo VulkanApi::createFbo(AbstractGraphicsApi::Device *d,
                                               FboLayout* lay,
                                               uint32_t w,uint32_t h,
                                               AbstractGraphicsApi::Texture *tcl,
                                               AbstractGraphicsApi::Texture *zbuf) {
  Detail::VDevice*            dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VFramebufferLayout* l =reinterpret_cast<Detail::VFramebufferLayout*>(lay);
  Detail::VTexture*           cl=reinterpret_cast<Detail::VTexture*>(tcl);
  Detail::VTexture*           tx=reinterpret_cast<Detail::VTexture*>(zbuf);

  return PFbo(new Detail::VFramebuffer(*dx,*l,w,h,*cl,*tx));
  }

AbstractGraphicsApi::PFbo VulkanApi::createFbo(AbstractGraphicsApi::Device *d,
                                               AbstractGraphicsApi::FboLayout* lay,
                                               uint32_t w, uint32_t h,
                                               AbstractGraphicsApi::Texture *tcl) {
  Detail::VDevice*            dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VTexture*           cl=reinterpret_cast<Detail::VTexture*>(tcl);
  Detail::VFramebufferLayout* l =reinterpret_cast<Detail::VFramebufferLayout*>(lay);

  return PFbo(new Detail::VFramebuffer(*dx,*l,w,h,*cl));
  }

AbstractGraphicsApi::PFboLayout VulkanApi::createFboLayout(AbstractGraphicsApi::Device *d,
                                                           uint32_t /*w*/, uint32_t /*h*/,
                                                           AbstractGraphicsApi::Swapchain *s,
                                                           Tempest::TextureFormat *att,
                                                           size_t attCount) {
  Detail::VDevice*     dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VSwapchain*  sx=reinterpret_cast<Detail::VSwapchain*>(s);

  VkFormat frm[256] = {};
  if(attCount>256)
    throw std::logic_error("more then 256 attachments is not implemented");

  for(size_t i=0;i<attCount;++i){
    frm[i] = Detail::nativeFormat(att[i]);
    }

  Detail::DSharedPtr<AbstractGraphicsApi::FboLayout*> impl{
    new Detail::VFramebufferLayout(*dx,*sx,frm,uint8_t(attCount))
    };

  return impl;
  }

AbstractGraphicsApi::PPipeline VulkanApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                         const RenderState &st,
                                                         const Decl::ComponentType *decl, size_t declSize,
                                                         size_t stride,
                                                         Topology tp,
                                                         const UniformsLayout &ulay,
                                                         std::shared_ptr<AbstractGraphicsApi::UniformsLay> &ulayImpl,
                                                         const std::initializer_list<AbstractGraphicsApi::Shader*> &sh) {
  Shader*const*        arr=sh.begin();
  Detail::VDevice*     dx =reinterpret_cast<Detail::VDevice*>(d);
  Detail::VShader*     vs =reinterpret_cast<Detail::VShader*>(arr[0]);
  Detail::VShader*     fs =reinterpret_cast<Detail::VShader*>(arr[1]);
  return PPipeline(new Detail::VPipeline(*dx,st,decl,declSize,stride,tp,ulay,ulayImpl,*vs,*fs));
  }

AbstractGraphicsApi::PShader VulkanApi::createShader(AbstractGraphicsApi::Device *d, const char *source, size_t src_size) {
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

AbstractGraphicsApi::CmdPool *VulkanApi::createCommandPool(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx =reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VCommandPool(*dx);
  }

AbstractGraphicsApi::PBuffer VulkanApi::createBuffer(AbstractGraphicsApi::Device *d,
                                                     const void *mem, size_t size,
                                                     MemUsage usage,BufferFlags flg) {
  Detail::VDevice* dx = reinterpret_cast<Detail::VDevice*>(d);

  if(flg==BufferFlags::Dynamic) {
    Detail::VBuffer stage=dx->allocator.alloc(mem,size,usage,BufferFlags::Dynamic);
    return PBuffer(new Detail::VBuffer(std::move(stage)));
    }
  if(flg==BufferFlags::Staging) {
    Detail::VBuffer stage=dx->allocator.alloc(mem,size,usage,BufferFlags::Staging);
    return PBuffer(new Detail::VBuffer(std::move(stage)));
    }
  else {
    Detail::VBuffer  stage=dx->allocator.alloc(mem,     size, MemUsage::TransferSrc,      BufferFlags::Staging);
    Detail::VBuffer  buf  =dx->allocator.alloc(nullptr, size, usage|MemUsage::TransferDst,BufferFlags::Static );

    Detail::DSharedPtr<Detail::VBuffer*> pstage(new Detail::VBuffer(std::move(stage)));
    Detail::DSharedPtr<Detail::VBuffer*> pbuf  (new Detail::VBuffer(std::move(buf)));

    Detail::VDevice::Data dat(*dx);
    dat.flush(*pstage.handler,size);
    dat.hold(pbuf);
    dat.hold(pstage); // preserve stage buffer, until gpu side copy is finished
    dat.copy(*pbuf.handler,*pstage.handler,size);
    dat.commit();

    //dx->waitData();
    return PBuffer(pbuf.handler);
    }
  }

AbstractGraphicsApi::PTexture VulkanApi::createTexture(AbstractGraphicsApi::Device *d, const Pixmap &p, TextureFormat frm, uint32_t mipCnt) {
  Detail::VDevice* dx     = reinterpret_cast<Detail::VDevice*>(d);
  const uint32_t   size   = p.dataSize();
  VkFormat         format = Detail::nativeFormat(frm);
  Detail::VBuffer  stage  = dx->allocator.alloc(p.data(),size,MemUsage::TransferSrc,BufferFlags::Staging);
  Detail::VTexture buf    = dx->allocator.alloc(p,mipCnt,format);

  Detail::DSharedPtr<Detail::VBuffer*>  pstage(new Detail::VBuffer (std::move(stage)));
  Detail::DSharedPtr<Detail::VTexture*> pbuf  (new Detail::VTexture(std::move(buf)));

  Detail::VDevice::Data dat(*dx);
  dat.hold(pstage);
  dat.hold(pbuf);

  dat.changeLayout(*pbuf.handler, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,mipCnt);
  if(isCompressedFormat(frm)){
    size_t blocksize  = (frm==TextureFormat::DXT1) ? 8 : 16;
    size_t bufferSize = 0;

    size_t w = size_t(p.w()), h = size_t(p.h());
    for(size_t i=0; i<mipCnt; i++){
      size_t blockcount = ((w+3)/4)*((h+3)/4);
      dat.copy(*pbuf.handler,w,h,i,*pstage.handler,bufferSize);

      bufferSize += blockcount*blocksize;
      w = std::max<size_t>(1,w/2);
      h = std::max<size_t>(1,h/2);
      }

    dat.changeLayout(*pbuf.handler, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipCnt);
    } else {
    dat.copy(*pbuf.handler,p.w(),p.h(),0,*pstage.handler,0);
    if(mipCnt>1)
      dat.generateMipmap(*pbuf.handler,format,p.w(),p.h(),mipCnt); else
      dat.changeLayout(*pbuf.handler, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipCnt);
    }
  dat.commit();

  //dx->waitData();
  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createTexture(AbstractGraphicsApi::Device *d, const uint32_t w, const uint32_t h, uint32_t mipCnt, TextureFormat frm) {
  Detail::VDevice* dx = reinterpret_cast<Detail::VDevice*>(d);
  
  Detail::VTexture buf=dx->allocator.alloc(w,h,mipCnt,frm);
  Detail::DSharedPtr<Detail::VTexture*> pbuf(new Detail::VTexture(std::move(buf)));

  Detail::VDevice::Data dat(*dx);
  VkImageLayout lay = VK_IMAGE_LAYOUT_UNDEFINED;
  if(isDepthFormat(frm)) {
    lay = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    } else {
    lay = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

  dat.hold(pbuf);
  dat.changeLayout(*pbuf.handler, Detail::nativeFormat(frm), VK_IMAGE_LAYOUT_UNDEFINED, lay, mipCnt);
  dat.commit();

  //dx->waitData();
  return PTexture(pbuf.handler);
  }

void VulkanApi::readPixels(AbstractGraphicsApi::Device *d, Pixmap& out, const PTexture t, TextureFormat frm,
                           const uint32_t w, const uint32_t h, uint32_t mip) {
  Detail::VDevice*  dx = reinterpret_cast<Detail::VDevice*>(d);
  Detail::VTexture* tx = reinterpret_cast<Detail::VTexture*>(t.handler);

  size_t         bpp  = 0;
  Pixmap::Format pfrm = Pixmap::Format::RGBA;
  switch(frm) {
    case TextureFormat::Undefined: bpp=0; break;
    case TextureFormat::Last:      bpp=0; break;
    case TextureFormat::Alpha:     bpp=1; pfrm = Pixmap::Format::A;   break;
    case TextureFormat::RGB8:      bpp=3; pfrm = Pixmap::Format::RGB; break;
    case TextureFormat::RGBA8:     bpp=4; break;
    case TextureFormat::RG16:      bpp=4; break;
    case TextureFormat::Depth16:   bpp=2; break;
    case TextureFormat::Depth24S8: bpp=4; break;
    case TextureFormat::Depth24x8: bpp=4; break;
    // TODO: dxt
    case TextureFormat::DXT1:      bpp=0; break;
    case TextureFormat::DXT3:      bpp=0; break;
    case TextureFormat::DXT5:      bpp=0; break;
    }

  const size_t    size  = w*h*bpp;
  Detail::VBuffer stage = dx->allocator.alloc(nullptr,size,MemUsage::TransferDst,BufferFlags::Staging);

  Detail::VDevice::Data dat(*dx);

  VkFormat format = Detail::nativeFormat(frm);
  dat.changeLayout(*tx, format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,     1);
  dat.copy(stage,w,h,mip,*tx,0);
  dat.changeLayout(*tx, format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);
  dat.commit();

  dx->waitData();

  out = Pixmap(w,h,pfrm);
  stage.read(out.data(),0,size);
  }

AbstractGraphicsApi::Desc *VulkanApi::createDescriptors(AbstractGraphicsApi::Device*      d,
                                                        const UniformsLayout&             lay,
                                                        AbstractGraphicsApi::UniformsLay* layP) {
  Detail::VDevice*               dx = reinterpret_cast<Detail::VDevice*>(d);
  Detail::VPipeline::VUboLayout* ux = reinterpret_cast<Detail::VPipeline::VUboLayout*>(layP);

  auto ret = Detail::VDescriptorArray::alloc(dx->device,lay,ux->impl);
  return ret;
  }

void VulkanApi::destroy(AbstractGraphicsApi::Desc *d) {
  Detail::VDescriptorArray* dx = reinterpret_cast<Detail::VDescriptorArray*>(d);
  Detail::VDescriptorArray::free(dx);
  }

std::shared_ptr<AbstractGraphicsApi::UniformsLay> VulkanApi::createUboLayout(Device *d, const UniformsLayout &lay) {
  Detail::VDevice* dx = reinterpret_cast<Detail::VDevice*>(d);
  return std::make_shared<Detail::VPipeline::VUboLayout>(dx->device,lay);
  }

AbstractGraphicsApi::CommandBuffer *VulkanApi::createCommandBuffer(AbstractGraphicsApi::Device *d,
                                                                   AbstractGraphicsApi::CmdPool *p,
                                                                   FboLayout *fbo,
                                                                   CmdType cmdType) {
  Detail::VDevice*             dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VCommandPool*        px=reinterpret_cast<Detail::VCommandPool*>(p);
  Detail::VFramebufferLayout*  fb=reinterpret_cast<Detail::VFramebufferLayout*>(fbo);
  return new Detail::VCommandBuffer(*dx,*px,fb,cmdType);
  }

uint32_t VulkanApi::nextImage(Device *d,
                              AbstractGraphicsApi::Swapchain *sw,
                              AbstractGraphicsApi::Semaphore *onReady) {
  Detail::VDevice*    dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VSwapchain* sx=reinterpret_cast<Detail::VSwapchain*>(sw);
  Detail::VSemaphore* rx=reinterpret_cast<Detail::VSemaphore*>(onReady);

  uint32_t id=uint32_t(-1);
  VkResult code=dx->nextImg(*sx,id,*rx);
  if(code==VK_ERROR_OUT_OF_DATE_KHR)
    throw DeviceLostException();

  if(code!=VK_SUCCESS && code!=VK_SUBOPTIMAL_KHR)
    throw std::runtime_error("failed to acquire swap chain image!");
  return id;
  }

AbstractGraphicsApi::Image *VulkanApi::getImage(AbstractGraphicsApi::Device*,
                                                AbstractGraphicsApi::Swapchain *sw,
                                                uint32_t id) {
  Detail::VSwapchain* sx=reinterpret_cast<Detail::VSwapchain*>(sw);
  Detail::VImage* img=&sx->images[id];
  return img;
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

  dx->waitData();
  std::lock_guard<std::mutex> g(dx->graphicsSync); // if dx->presentQueue==dx->graphicsQueue
  VkResult code = vkQueuePresentKHR(dx->presentQueue,&presentInfo);
  if(code==VK_ERROR_OUT_OF_DATE_KHR || code==VK_SUBOPTIMAL_KHR) {
    //todo
    throw DeviceLostException();
    } else
  if(code!=VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
    }
  }

void VulkanApi::draw(Device *d,
                     CommandBuffer *cmd,
                     Semaphore *wait,
                     Semaphore *onReady,
                     Fence *onReadyCpu) {
  Detail::VDevice*        dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VCommandBuffer* cx=reinterpret_cast<Detail::VCommandBuffer*>(cmd);
  auto*                   wx=reinterpret_cast<const Detail::VSemaphore*>(wait);
  auto*                   rx=reinterpret_cast<const Detail::VSemaphore*>(onReady);
  auto*                   rc=reinterpret_cast<const Detail::VFence*>(onReadyCpu);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore          waitSemaphores[] = {wx->impl};
  VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores    = waitSemaphores;
  submitInfo.pWaitDstStageMask  = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cx->impl;

  if(rx!=nullptr) {
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &rx->impl;
    }

  if(onReadyCpu!=nullptr)
    onReadyCpu->reset();

  dx->submitQueue(dx->graphicsQueue,submitInfo,rc==nullptr ? VK_NULL_HANDLE : rc->impl,true);
  }

void VulkanApi::draw(AbstractGraphicsApi::Device *d,
                     AbstractGraphicsApi::CommandBuffer **cmd, size_t count,
                     Semaphore **wait, size_t waitCnt,
                     Semaphore **done, size_t doneCnt,
                     Fence     *doneCpu) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);

  std::lock_guard<std::mutex> guard(impl->syncBuf);
  auto& cmdBuf   = impl->cmdBuf;
  auto& semBuf   = impl->semBuf;

  cmdBuf.resize(count);
  for(size_t i=0;i<count;++i) {
    auto* cx=reinterpret_cast<Detail::VCommandBuffer*>(cmd[i]);
    cmdBuf[i] = cx->impl;
    }

  semBuf.resize(waitCnt+doneCnt);
  for(size_t i=0;i<waitCnt;++i) {
    auto* sx=reinterpret_cast<Detail::VSemaphore*>(wait[i]);
    semBuf[i] = sx->impl;
    }
  for(size_t i=0;i<doneCnt;++i) {
    auto* sx=reinterpret_cast<Detail::VSemaphore*>(done[i]);
    semBuf[i+waitCnt] = sx->impl;
    }

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};//VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  submitInfo.waitSemaphoreCount   = waitCnt;
  submitInfo.pWaitSemaphores      = semBuf.data();
  submitInfo.pWaitDstStageMask    = waitStages;

  submitInfo.signalSemaphoreCount = doneCnt;
  submitInfo.pSignalSemaphores    = semBuf.data()+waitCnt;

  submitInfo.commandBufferCount   = count;
  submitInfo.pCommandBuffers      = cmdBuf.data();

  if(doneCpu!=nullptr)
    doneCpu->reset();
  auto* rc=reinterpret_cast<Detail::VFence*>(doneCpu);

  dx->submitQueue(dx->graphicsQueue,submitInfo,rc==nullptr ? VK_NULL_HANDLE : rc->impl,true);
  }

void VulkanApi::getCaps(Device *d,Caps &caps) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  caps=dx->caps;
  }

