#include "vulkanapi.h"

#include "vulkan/vdevice.h"
#include "vulkan/vswapchain.h"
#include "vulkan/vrenderpass.h"
#include "vulkan/vpipeline.h"
#include "vulkan/vframebuffer.h"
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

using namespace Tempest;


struct VulkanApi::Impl : public Detail::VulkanApi {
  using VulkanApi::VulkanApi; 
  using DeviceMemory=VkDeviceMemory;

  DeviceMemory alloc(size_t size, uint32_t typeBits);
  void         free(DeviceMemory m);

  std::vector<VkCommandBuffer> cmdBuf;
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

void VulkanApi::destroy(AbstractGraphicsApi::Swapchain *d) {
  Detail::VSwapchain* s=reinterpret_cast<Detail::VSwapchain*>(d);
  delete s;
  }

AbstractGraphicsApi::Pass *VulkanApi::createPass(AbstractGraphicsApi::Device *d,
                                                 AbstractGraphicsApi::Swapchain *s,
                                                 TextureFormat zformat,
                                                 FboMode fcolor, const Color *clear,
                                                 FboMode fzbuf, const float *zclear) {
  static VkFormat zfrm=VkFormat::VK_FORMAT_UNDEFINED;
  if(zformat==TextureFormat::Depth16)
    zfrm=VkFormat::VK_FORMAT_D16_UNORM;

  Detail::VDevice*    dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VSwapchain* sx=reinterpret_cast<Detail::VSwapchain*>(s);
  return new Detail::VRenderPass(*dx,sx->format(),zfrm,fcolor,clear,fzbuf,zclear);
  }

void VulkanApi::destroy(AbstractGraphicsApi::Pass *pass) {
  Detail::VRenderPass* px=reinterpret_cast<Detail::VRenderPass*>(pass);
  delete px;
  }

AbstractGraphicsApi::Fbo *VulkanApi::createFbo(AbstractGraphicsApi::Device *d,
                                               AbstractGraphicsApi::Swapchain *s,
                                               AbstractGraphicsApi::Pass *pass,
                                               uint32_t imageId) {
  Detail::VDevice*     dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VSwapchain*  sx=reinterpret_cast<Detail::VSwapchain*>(s);
  Detail::VRenderPass* px=reinterpret_cast<Detail::VRenderPass*>(pass);

  return new Detail::VFramebuffer(*dx,*px,*sx,imageId);
  }

AbstractGraphicsApi::Fbo *VulkanApi::createFbo(AbstractGraphicsApi::Device *d,
                                               AbstractGraphicsApi::Swapchain *s,
                                               AbstractGraphicsApi::Pass *pass,
                                               uint32_t imageId,
                                               AbstractGraphicsApi::Texture *zbuf) {
  Detail::VDevice*     dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VSwapchain*  sx=reinterpret_cast<Detail::VSwapchain*>(s);
  Detail::VRenderPass* px=reinterpret_cast<Detail::VRenderPass*>(pass);
  Detail::VTexture*    tx=reinterpret_cast<Detail::VTexture*>(zbuf);

  return new Detail::VFramebuffer(*dx,*px,*sx,imageId,*tx);
  }

void VulkanApi::destroy(AbstractGraphicsApi::Fbo *pass) {
  Detail::VFramebuffer* px=reinterpret_cast<Detail::VFramebuffer*>(pass);
  delete px;
  }

AbstractGraphicsApi::Pipeline *VulkanApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                         AbstractGraphicsApi::Pass *p,
                                                         uint32_t width, uint32_t height,
                                                         const RenderState &st,
                                                         const Decl::ComponentType *decl, size_t declSize,
                                                         size_t stride,
                                                         Topology tp,
                                                         const UniformsLayout &ulay,
                                                         std::shared_ptr<AbstractGraphicsApi::UniformsLay> &ulayImpl,
                                                         const std::initializer_list<AbstractGraphicsApi::Shader*> &sh) {
  Shader*const*        arr=sh.begin();
  Detail::VDevice*     dx =reinterpret_cast<Detail::VDevice*>(d);
  Detail::VRenderPass* px =reinterpret_cast<Detail::VRenderPass*>(p);
  Detail::VShader*     vs =reinterpret_cast<Detail::VShader*>(arr[0]);
  Detail::VShader*     fs =reinterpret_cast<Detail::VShader*>(arr[1]);
  return new Detail::VPipeline(*dx,*px,width,height,st,decl,declSize,stride,tp,ulay,ulayImpl,*vs,*fs);
  }

AbstractGraphicsApi::Shader *VulkanApi::createShader(AbstractGraphicsApi::Device *d, const char *source, size_t src_size) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VShader(*dx,source,src_size);
  }

void VulkanApi::destroy(AbstractGraphicsApi::Shader *shader) {
  Detail::VShader* sx=reinterpret_cast<Detail::VShader*>(shader);
  delete sx;
  }

AbstractGraphicsApi::Fence *VulkanApi::createFence(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx =reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VFence(*dx);
  }

void VulkanApi::destroy(AbstractGraphicsApi::Fence *f) {
  Detail::VFence* fx=reinterpret_cast<Detail::VFence*>(f);
  delete fx;
  }

AbstractGraphicsApi::Semaphore *VulkanApi::createSemaphore(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VSemaphore(*dx);
  }

void VulkanApi::destroy(AbstractGraphicsApi::Semaphore *s) {
  Detail::VSemaphore* sx=reinterpret_cast<Detail::VSemaphore*>(s);
  delete sx;
  }

AbstractGraphicsApi::CmdPool *VulkanApi::createCommandPool(AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx =reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VCommandPool(*dx);
  }

void VulkanApi::destroy(AbstractGraphicsApi::CmdPool *cmd) {
  Detail::VCommandPool* cx=reinterpret_cast<Detail::VCommandPool*>(cmd);
  delete cx;
  }

AbstractGraphicsApi::Buffer *VulkanApi::createBuffer(AbstractGraphicsApi::Device *d,
                                                     const void *mem, size_t size,
                                                     MemUsage usage,BufferFlags flg) {
  Detail::VDevice* dx = reinterpret_cast<Detail::VDevice*>(d);
  
  std::lock_guard<std::mutex> guard(dx->allocSync);
  if(flg==BufferFlags::Dynamic) {
    Detail::VBuffer stage=dx->allocator.alloc(mem,size,usage,BufferFlags::Dynamic);
    return new Detail::VBuffer(std::move(stage));
    }
  if(flg==BufferFlags::Staging) {
    Detail::VBuffer stage=dx->allocator.alloc(mem,size,usage,BufferFlags::Staging);
    return new Detail::VBuffer(std::move(stage));
    }
  else {
    Detail::VBuffer  stage=dx->allocator.alloc(mem,     size, MemUsage::TransferSrc,       BufferFlags::Staging);
    Detail::VBuffer  buf  =dx->allocator.alloc(nullptr, size, usage|MemUsage::TransferDst, BufferFlags::Static );

    dx->copy(buf,stage,size);
    dx->waitData();
    return new Detail::VBuffer(std::move(buf));
    }
  }

void VulkanApi::destroy(AbstractGraphicsApi::Buffer *cmd) {
  Detail::VBuffer* cx=reinterpret_cast<Detail::VBuffer*>(cmd);
  delete cx;
  }

AbstractGraphicsApi::Texture *VulkanApi::createTexture(AbstractGraphicsApi::Device *d, const Pixmap &pref, bool mips) {
  Detail::VDevice*   dx = reinterpret_cast<Detail::VDevice*>(d);

  const Pixmap* p=&pref;
  Pixmap alt;
  if( p->bpp()==3 && !dx->caps.rgb8 ){
    alt = Pixmap(pref,Pixmap::Format::RGBA);
    p   = &alt;
    }
  const uint32_t  mipCnt = mips ? mipCount(p->w(),p->h()) : 1;
  const uint32_t  size =p->w()*p->h()*p->bpp();
  
  std::lock_guard<std::mutex> guard(dx->allocSync);
  Detail::VBuffer  stage=dx->allocator.alloc(p->data(),size,MemUsage::TransferSrc,BufferFlags::Staging);
  Detail::VTexture buf  =dx->allocator.alloc(*p,mipCnt);
  dx->changeLayout(buf, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,mipCnt);
  dx->copy(buf,p->w(),p->h(),stage);
  if(mipCnt>1)
    dx->generateMipmap(buf,VK_FORMAT_R8G8B8A8_UNORM,p->w(),p->h(),mipCnt); else
    dx->changeLayout(buf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipCnt);
  dx->waitData();
  return new Detail::VTexture(std::move(buf));
  }

AbstractGraphicsApi::Texture *VulkanApi::createTexture(AbstractGraphicsApi::Device *d, const uint32_t w, const uint32_t h, bool mips, TextureFormat frm) {
  Detail::VDevice*   dx = reinterpret_cast<Detail::VDevice*>(d);
  const uint32_t mipCnt = mips ? mipCount(w,h) : 1;
  
  std::lock_guard<std::mutex> guard(dx->allocSync);
  auto buf=dx->allocator.alloc(w,h,mipCnt,frm);
  dx->changeLayout(buf, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL/*TODO*/,mipCnt);
  return new Detail::VTexture(std::move(buf));
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

AbstractGraphicsApi::CommandBuffer *VulkanApi::createCommandBuffer(AbstractGraphicsApi::Device *d,CmdPool *p,CmdType cmdType) {
  Detail::VDevice*      dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VCommandPool* px=reinterpret_cast<Detail::VCommandPool*>(p);
  return new Detail::VCommandBuffer(*dx,*px,cmdType);
  }

void VulkanApi::destroy(AbstractGraphicsApi::CommandBuffer *cmd) {
  Detail::VCommandBuffer* cx=reinterpret_cast<Detail::VCommandBuffer*>(cmd);
  delete cx;
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
                     Swapchain *,
                     CommandBuffer *cmd,
                     Semaphore *wait,
                     Semaphore *onReady,
                     Fence *onReadyCpu) {
  Detail::VDevice*        dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VCommandBuffer* cx=reinterpret_cast<Detail::VCommandBuffer*>(cmd);
  auto*                   wx=reinterpret_cast<const Detail::VSemaphore*>(wait);
  auto*                   rx=reinterpret_cast<const Detail::VSemaphore*>(onReady);
  auto*                   rc=reinterpret_cast<const Detail::VFence*>(onReadyCpu);

  //dx->waitData();

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
  dx->submitQueue(dx->graphicsQueue,submitInfo,rc==nullptr ? VK_NULL_HANDLE : rc->impl);
  }

void VulkanApi::draw(AbstractGraphicsApi::Device *d,
                     AbstractGraphicsApi::Swapchain*,
                     AbstractGraphicsApi::CommandBuffer **cmd, size_t count,
                     AbstractGraphicsApi::Semaphore *wait,
                     AbstractGraphicsApi::Semaphore *onReady,
                     AbstractGraphicsApi::Fence *onReadyCpu) {
  Detail::VDevice*        dx=reinterpret_cast<Detail::VDevice*>(d);
  auto*                   wx=reinterpret_cast<const Detail::VSemaphore*>(wait);
  auto*                   rx=reinterpret_cast<const Detail::VSemaphore*>(onReady);
  auto*                   rc=reinterpret_cast<const Detail::VFence*>(onReadyCpu);

  auto& cmdBuf = impl->cmdBuf;
  cmdBuf.resize(count);
  for(size_t i=0;i<count;++i){
    Detail::VCommandBuffer* cx=reinterpret_cast<Detail::VCommandBuffer*>(cmd[i]);
    cmdBuf[i] = cx->impl;
    }

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore          waitSemaphores[] = {wx->impl};
  VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores    = waitSemaphores;
  submitInfo.pWaitDstStageMask  = waitStages;

  submitInfo.commandBufferCount = count;
  submitInfo.pCommandBuffers    = cmdBuf.data();

  if(rx!=nullptr) {
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &rx->impl;
    }

  if(onReadyCpu!=nullptr)
    onReadyCpu->reset();
  dx->submitQueue(dx->graphicsQueue,submitInfo,rc==nullptr ? VK_NULL_HANDLE : rc->impl);
  }

void VulkanApi::getCaps(Device *d,Caps &caps) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  caps=dx->caps;
  }

uint32_t VulkanApi::mipCount(uint32_t w, uint32_t h) {
  return uint32_t(std::floor(std::log2(std::max(w,h)))) + 1;
  }
