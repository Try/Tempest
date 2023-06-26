#if defined(TEMPEST_BUILD_VULKAN)

#include "vswapchain.h"

#include <Tempest/Application>
#include <Tempest/SystemApi>

#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VSwapchain::FenceList::FenceList(VkDevice dev, uint32_t cnt)
  :dev(dev), size(0) {
  acquire.reset(new VkFence[cnt]);
  for(uint32_t i=0; i<cnt; ++i)
    acquire [i] = VK_NULL_HANDLE;

  try {
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(uint32_t i=0; i<cnt; ++i) {
      vkAssert(vkCreateFence(dev,&fenceInfo,nullptr,&acquire [i]));
      ++size;
      }
    }
  catch(...) {
    for(uint32_t i=0; i<cnt; ++i) {
      if(acquire[i]!=VK_NULL_HANDLE)
        vkDestroyFence(dev,acquire[i],nullptr);
      }
    throw;
    }
  }

VSwapchain::FenceList::FenceList(VSwapchain::FenceList&& oth)
  :dev(oth.dev), size(oth.size) {
  acquire   = std::move(oth.acquire);
  oth.size = 0;
  }

VSwapchain::FenceList& VSwapchain::FenceList::operator =(VSwapchain::FenceList&& oth) {
  std::swap(dev,     oth.dev);
  std::swap(acquire, oth.acquire);
  std::swap(size,    oth.size);
  return *this;
  }

VSwapchain::FenceList::~FenceList() {
  for(uint32_t i=0; i<size; ++i)
    vkDestroyFence(dev,acquire[i],nullptr);
  }

VSwapchain::VSwapchain(VDevice &device, SystemApi::Window* hwnd)
  :device(device), hwnd(hwnd) {
  try {
    surface = device.createSurface(hwnd);

    auto     support  = device.querySwapChainSupport(surface);
    uint32_t imgCount = findImageCount(support);
    createSwapchain(device,support,SystemApi::windowClientRect(hwnd),imgCount);
    }
  catch(...) {
    cleanup();
    throw;
    }
  }

VSwapchain::~VSwapchain() {
  cleanup();
  }

void VSwapchain::cleanupSwapchain() noexcept {
  // aquire is not a 'true' queue operation - have to wait explicitly on it
  vkWaitForFences(device.device.impl,fence.size,fence.acquire.get(), VK_TRUE,std::numeric_limits<uint64_t>::max());
  // wait for vkQueuePresent to finish, so we can delete semaphores
  // NOTE: maybe update to VK_KHR_present_wait ?
  device.presentQueue->waitIdle();
  fence = FenceList();

  for(auto imageView : views)
    if(map!=nullptr && imageView!=VK_NULL_HANDLE)
      map->notifyDestroy(imageView);
  map = nullptr;
  for(auto imageView : views)
    if(imageView!=VK_NULL_HANDLE)
      vkDestroyImageView(device.device.impl,imageView,nullptr);
  views.clear();

  for(auto& s : sync) {
    if(s.acquire!=VK_NULL_HANDLE)
      vkDestroySemaphore(device.device.impl,s.acquire,nullptr);
    if(s.present!=VK_NULL_HANDLE)
      vkDestroySemaphore(device.device.impl,s.present,nullptr);
    }
  sync.clear();

  images.clear();
  if(swapChain!=VK_NULL_HANDLE)
    vkDestroySwapchainKHR(device.device.impl,swapChain,nullptr);

  swapChain            = VK_NULL_HANDLE;
  swapChainImageFormat = VK_FORMAT_UNDEFINED;
  swapChainExtent      = {};
  }

void VSwapchain::cleanupSurface() noexcept {
  if(surface!=VK_NULL_HANDLE)
    vkDestroySurfaceKHR(device.instance,surface,nullptr);
  surface = VK_NULL_HANDLE;
  }

void VSwapchain::reset() {
  cleanupSwapchain();

  Rect rect = SystemApi::windowClientRect(hwnd);
  if(rect.isEmpty())
    return;

  try {
    auto     support  = device.querySwapChainSupport(surface);
    uint32_t imgCount = findImageCount(support);
    createSwapchain(device,support,rect,imgCount);
    }
  catch(...) {
    cleanupSwapchain();
    throw;
    }
  }

void VSwapchain::cleanup() noexcept {
  cleanupSwapchain();
  cleanupSurface();
  }

void VSwapchain::createSwapchain(VDevice& device, const SwapChainSupport& swapChainSupport,
                                 const Rect& rect, uint32_t imgCount) {
  VkBool32 support=false;
  vkGetPhysicalDeviceSurfaceSupportKHR(device.physicalDevice,device.presentQueue->family,surface,&support);
  if(!support)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  VkSurfaceFormatKHR surfaceFormat = findSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR   presentMode   = findSwapPresentMode  (swapChainSupport.presentModes);
  VkExtent2D         extent        = findSwapExtent       (swapChainSupport.capabilities,uint32_t(rect.w),uint32_t(rect.h));

  VkSwapchainCreateInfoKHR createInfo = {};
  createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface          = surface;
  createInfo.minImageCount    = imgCount;
  createInfo.imageFormat      = surfaceFormat.format;
  createInfo.imageColorSpace  = surfaceFormat.colorSpace;
  createInfo.imageExtent      = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  createInfo.imageUsage      &= swapChainSupport.capabilities.supportedUsageFlags;

  if(device.graphicsQueue!=device.presentQueue) {
    const uint32_t qidx[]  = {device.graphicsQueue->family,device.presentQueue->family};
    createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices   = qidx;
    } else {
    createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

  createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode    = presentMode;
  createInfo.clipped        = VK_FALSE;

  if(vkCreateSwapchainKHR(device.device.impl, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent      = extent;

  createImageViews(device);

  VkSemaphoreCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.flags = 0;
  sync.resize(views.size());
  for(auto& i:sync) {
    vkAssert(vkCreateSemaphore(device.device.impl,&info,nullptr,&i.acquire));
    vkAssert(vkCreateSemaphore(device.device.impl,&info,nullptr,&i.present));
    }
  fence = FenceList(device.device.impl,uint32_t(views.size()));
  acquireNextImage();
  }

void VSwapchain::createImageViews(VDevice &device) {
  uint32_t imgCount=0;
  vkGetSwapchainImagesKHR(device.device.impl, swapChain, &imgCount, nullptr);
  images.resize(imgCount);
  vkGetSwapchainImagesKHR(device.device.impl, swapChain, &imgCount, images.data());

  views.resize(images.size());

  for(size_t i=0; i<images.size(); i++) {
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = images[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = swapChainImageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel   = 0;
    createInfo.subresourceRange.levelCount     = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount     = 1;

    if(vkCreateImageView(device.device.impl,&createInfo,nullptr,&views[i])!=VK_SUCCESS)
      throw std::system_error(Tempest::GraphicsErrc::NoDevice);
    }
  }

VkSurfaceFormatKHR VSwapchain::findSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
  if(availableFormats.size()==1 && availableFormats[0].format==VK_FORMAT_UNDEFINED)
    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  for(const auto& availableFormat : availableFormats) {
    if(availableFormat.format==VK_FORMAT_B8G8R8A8_UNORM &&
       availableFormat.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return availableFormat;
    }

  return availableFormats[0];
  }

VkPresentModeKHR VSwapchain::findSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
  /** intel says mailbox is better option for games
    * https://software.intel.com/content/www/us/en/develop/articles/api-without-secrets-introduction-to-vulkan-part-2.html
    **/
  std::initializer_list<VkPresentModeKHR> modes={
    VK_PRESENT_MODE_FIFO_KHR,         // vsync; optimal
    VK_PRESENT_MODE_MAILBOX_KHR,      // vsync; wait until vsync
    VK_PRESENT_MODE_FIFO_RELAXED_KHR, // vsync; optimal, but tearing
    VK_PRESENT_MODE_IMMEDIATE_KHR,    // no vsync
    };

  for(auto mode:modes){
    for(const auto available:availablePresentModes)
      if(available==mode)
        return mode;
    }

  // no good modes
  return VK_PRESENT_MODE_FIFO_KHR;
  }

VkExtent2D VSwapchain::findSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                      uint32_t w,uint32_t h) {
  if(capabilities.currentExtent.width!=std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
    }

  VkExtent2D actualExtent = {
    static_cast<uint32_t>(w),
    static_cast<uint32_t>(h)
    };

  actualExtent.width  = std::clamp(actualExtent.width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width );
  actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

  return actualExtent;
  }

uint32_t VSwapchain::findImageCount(const SwapChainSupport& support) const {
  const uint32_t maxImages = support.capabilities.maxImageCount==0 ? uint32_t(-1) : support.capabilities.maxImageCount;
  const uint32_t minImages = support.capabilities.minImageCount;

  /* NOTE: https://github.com/KhronosGroup/Vulkan-Docs/issues/1158#issuecomment-573874821
   * It's not clear how many images make a good fit
   */
  uint32_t imageCount = minImages + 1;

  imageCount = std::clamp(imageCount, minImages, maxImages);
  return imageCount;
  }

void VSwapchain::acquireNextImage() {
  size_t sId = 0;
  for(size_t i=0; i<sync.size(); ++i)
    if(sync[i].imgId==imgIndex) {
      sId = i;
      break;
      }
  auto&    slot = sync[sId];
  auto&    f    = fence.acquire[sId];

  vkWaitForFences(device.device.impl,1,&f,VK_TRUE,std::numeric_limits<uint64_t>::max());
  vkResetFences(device.device.impl,1,&f);

  uint32_t id   = uint32_t(-1);
  VkResult code = vkAcquireNextImageKHR(device.device.impl,
                                        swapChain,
                                        std::numeric_limits<uint64_t>::max(),
                                        slot.acquire,
                                        f,
                                        &id);
  if(code==VK_ERROR_OUT_OF_DATE_KHR)
    throw DeviceLostException();
  if(code==VK_SUBOPTIMAL_KHR)
    throw SwapchainSuboptimal();
  if(code!=VK_SUCCESS)
    vkAssert(code);

  imgIndex   = id;
  slot.imgId = id;
  slot.state = S_Pending;
  }

uint32_t VSwapchain::currentBackBufferIndex() {
  return imgIndex;
  }

void VSwapchain::present() {
  size_t sId = 0;
  for(size_t i=0; i<sync.size(); ++i)
    if(sync[i].imgId==imgIndex) {
      sId = i;
      break;
      }

  auto& slot = sync[sId];
  if(device.vkQueueSubmit2!=nullptr) {
    VkSemaphoreSubmitInfoKHR signal = {};
    signal.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    signal.semaphore = slot.present;
    signal.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSubmitInfo2KHR submitInfo = {};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos    = &signal;

    device.graphicsQueue->submit(1, &submitInfo, VK_NULL_HANDLE, device.vkQueueSubmit2);
    } else {
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &slot.present;
    device.graphicsQueue->submit(1, &submitInfo, VK_NULL_HANDLE);
    }

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = &slot.present;
  presentInfo.swapchainCount     = 1;
  presentInfo.pSwapchains        = &swapChain;
  presentInfo.pImageIndices      = &imgIndex;

  slot.imgId = uint32_t(-1);
  slot.state = S_Idle;

  auto tx = Application::tickCount();
  VkResult code = device.presentQueue->present(presentInfo);
  if(code==VK_ERROR_OUT_OF_DATE_KHR || code==VK_SUBOPTIMAL_KHR)
    throw SwapchainSuboptimal();
  tx = Application::tickCount()-tx;
  if(tx > 2) {
    // std::chrono::system_clock::time_point p = std::chrono::system_clock::now();
    // time_t t = std::chrono::system_clock::to_time_t(p);
    // char str[26] = {};
    // strftime(str, sizeof(str), "%H:%M.%S", localtime(&t));
    // Log::i(str," : vkQueuePresentKHR[",imgIndex,"] = ", tx);
    }
  Detail::vkAssert(code);

  acquireNextImage();
  }

#endif

