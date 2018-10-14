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
#include "deviceallocator.h"

#include <vulkan/vulkan.hpp>

using namespace Tempest;


struct VulkanApi::Impl : public Detail::VulkanApi {
  using VulkanApi::VulkanApi; 
  using DeviceMemory=VkDeviceMemory;

  DeviceMemory alloc(size_t size, uint32_t typeBits);
  void         free(DeviceMemory m);
  };

VulkanApi::VulkanApi() {
  impl.reset(new Impl(true));
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
                                                 AbstractGraphicsApi::Swapchain *s) {
  Detail::VDevice*    dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VSwapchain* sx=reinterpret_cast<Detail::VSwapchain*>(s);
  return new Detail::VRenderPass(*dx,sx->format());
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

void VulkanApi::destroy(AbstractGraphicsApi::Fbo *pass) {
  Detail::VFramebuffer* px=reinterpret_cast<Detail::VFramebuffer*>(pass);
  delete px;
  }

AbstractGraphicsApi::Pipeline *VulkanApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                         AbstractGraphicsApi::Pass *p,
                                                         uint32_t width, uint32_t height,
                                                         const std::initializer_list<AbstractGraphicsApi::Shader*> &sh) {
  Shader*const*        arr=sh.begin();
  Detail::VDevice*     dx =reinterpret_cast<Detail::VDevice*>(d);
  Detail::VRenderPass* px =reinterpret_cast<Detail::VRenderPass*>(p);
  Detail::VShader*     vs =reinterpret_cast<Detail::VShader*>(arr[0]);
  Detail::VShader*     fs =reinterpret_cast<Detail::VShader*>(arr[1]);
  return new Detail::VPipeline(*dx,*px,width,height,*vs,*fs);
  }

void VulkanApi::destroy(AbstractGraphicsApi::Pipeline *pass) {
  Detail::VPipeline* px=reinterpret_cast<Detail::VPipeline*>(pass);
  delete px;
  }

AbstractGraphicsApi::Shader *VulkanApi::createShader(AbstractGraphicsApi::Device *d, const char *file) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VShader(*dx,file);
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
                                                     const void *mem, size_t size, MemUsage usage) {
  Detail::VDevice* dx =reinterpret_cast<Detail::VDevice*>(d);
  Detail::VBuffer  buf=dx->allocator.alloc(mem,size,usage);

  return new  Detail::VBuffer(std::move(buf));
  }

void VulkanApi::destroy(AbstractGraphicsApi::Buffer *cmd) {
  Detail::VBuffer* cx=reinterpret_cast<Detail::VBuffer*>(cmd);
  delete cx;
  }

AbstractGraphicsApi::CommandBuffer *VulkanApi::createCommandBuffer(AbstractGraphicsApi::Device *d,CmdPool *p) {
  Detail::VDevice*      dx=reinterpret_cast<Detail::VDevice*>(d);
  Detail::VCommandPool* px=reinterpret_cast<Detail::VCommandPool*>(p);
  return new Detail::VCommandBuffer(*dx,*px);
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
  if(code==VK_ERROR_OUT_OF_DATE_KHR) {
    //todo
    throw std::runtime_error("lost device");
    }

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
    throw std::runtime_error("lost device");
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
  //Detail::VSwapchain*     sx=reinterpret_cast<Detail::VSwapchain*>(sw);
  Detail::VCommandBuffer* cx=reinterpret_cast<Detail::VCommandBuffer*>(cmd);
  auto*                   wx=reinterpret_cast<const Detail::VSemaphore*>(wait);
  auto*                   rx=reinterpret_cast<const Detail::VSemaphore*>(onReady);
  auto*                   rc=reinterpret_cast<const Detail::VFence*>(onReadyCpu);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore          waitSemaphores[] = {wx->impl};
  VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores   = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cx->impl;

  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = &rx->impl;

  onReadyCpu->reset();
  if(vkQueueSubmit(dx->graphicsQueue,1,&submitInfo,rc->impl)!=VK_SUCCESS)
    throw std::runtime_error("failed to submit draw command buffer!");
  }
