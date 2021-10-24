#if defined(TEMPEST_BUILD_VULKAN)

#include "vswapchain.h"

#include <Tempest/SystemApi>

#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VSwapchain::FenceList::FenceList(VkDevice dev, uint32_t cnt)
  :dev(dev), size(0) {
  aquire .reset(new VkFence[cnt]);
  present.reset(new VkFence[cnt]);
  for(uint32_t i=0; i<cnt; ++i) {
    aquire [i] = VK_NULL_HANDLE;
    present[i] = VK_NULL_HANDLE;
    }

  try {
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(uint32_t i=0; i<cnt; ++i) {
      vkAssert(vkCreateFence(dev,&fenceInfo,nullptr,&aquire [i]));
      vkAssert(vkCreateFence(dev,&fenceInfo,nullptr,&present[i]));
      ++size;
      }
    }
  catch(...) {
    for(uint32_t i=0; i<cnt; ++i) {
      if(aquire[i]!=VK_NULL_HANDLE)
        vkDestroyFence(dev,aquire [i],nullptr);
      if(present[i]!=VK_NULL_HANDLE)
        vkDestroyFence(dev,present[i],nullptr);
      }
    throw;
    }
  }

VSwapchain::FenceList::FenceList(VSwapchain::FenceList&& oth)
  :dev(oth.dev), size(oth.size) {
  aquire   = std::move(oth.aquire);
  present  = std::move(oth.present);
  oth.size = 0;
  }

VSwapchain::FenceList& VSwapchain::FenceList::operator =(VSwapchain::FenceList&& oth) {
  std::swap(dev,     oth.dev);
  std::swap(aquire,  oth.aquire);
  std::swap(present, oth.present);
  std::swap(size,    oth.size);
  return *this;
  }

VSwapchain::FenceList::~FenceList() {
  for(uint32_t i=0; i<size; ++i) {
    vkDestroyFence(dev,aquire [i],nullptr);
    vkDestroyFence(dev,present[i],nullptr);
    }
  }

VSwapchain::VSwapchain(VDevice &device, SystemApi::Window* hwnd)
  :device(device), hwnd(hwnd) {
  try {
    surface = device.createSurface(hwnd);

    auto     support  = device.querySwapChainSupport(surface);
    uint32_t imgCount = getImageCount(support);
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
  vkWaitForFences(device.device.impl,fence.size,fence.aquire.get(), VK_TRUE,std::numeric_limits<uint64_t>::max());
  vkWaitForFences(device.device.impl,fence.size,fence.present.get(),VK_TRUE,std::numeric_limits<uint64_t>::max());
  fence = FenceList();

  for(auto imageView : views)
    if(map!=nullptr && imageView!=VK_NULL_HANDLE)
      map->notifyDestroy(imageView);

  for(auto imageView : views)
    if(imageView!=VK_NULL_HANDLE)
      vkDestroyImageView(device.device.impl,imageView,nullptr);
  for(auto s : sync) {
    if(s.aquire!=VK_NULL_HANDLE)
      vkDestroySemaphore(device.device.impl,s.aquire,nullptr);
    if(s.present!=VK_NULL_HANDLE)
      vkDestroySemaphore(device.device.impl,s.present,nullptr);
    }

  views.clear();
  images.clear();
  sync.clear();

  if(swapChain!=VK_NULL_HANDLE)
    vkDestroySwapchainKHR(device.device.impl,swapChain,nullptr);

  swapChain            = VK_NULL_HANDLE;
  swapChainImageFormat = VK_FORMAT_UNDEFINED;
  swapChainExtent      = {};
  }

void VSwapchain::cleanupSurface() noexcept {
  if(surface!=VK_NULL_HANDLE)
    vkDestroySurfaceKHR(device.instance,surface,nullptr);
  }

void VSwapchain::reset() {
  const Rect rect = SystemApi::windowClientRect(hwnd);
  if(rect.isEmpty())
    return;
  cleanupSwapchain();

  try {
    auto     support  = device.querySwapChainSupport(surface);
    uint32_t imgCount = getImageCount(support);
    createSwapchain(device,support,rect,imgCount);
    }
  catch(...) {
    cleanup();
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

  VkSurfaceFormatKHR surfaceFormat = getSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR   presentMode   = getSwapPresentMode  (swapChainSupport.presentModes);
  VkExtent2D         extent        = getSwapExtent       (swapChainSupport.capabilities,uint32_t(rect.w),uint32_t(rect.h));

  VkSwapchainCreateInfoKHR createInfo = {};
  createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface          = surface;
  createInfo.minImageCount    = imgCount;
  createInfo.imageFormat      = surfaceFormat.format;
  createInfo.imageColorSpace  = surfaceFormat.colorSpace;
  createInfo.imageExtent      = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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
  createInfo.clipped        = VK_TRUE;

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
    vkAssert(vkCreateSemaphore(device.device.impl,&info,nullptr,&i.aquire));
    vkAssert(vkCreateSemaphore(device.device.impl,&info,nullptr,&i.present));
    }
  fence = FenceList(device.device.impl,views.size());
  aquireNextImage();
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

VkSurfaceFormatKHR VSwapchain::getSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
  if(availableFormats.size()==1 && availableFormats[0].format==VK_FORMAT_UNDEFINED)
    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  for(const auto& availableFormat : availableFormats) {
    if(availableFormat.format==VK_FORMAT_B8G8R8A8_UNORM &&
       availableFormat.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return availableFormat;
    }

  return availableFormats[0];
  }


VkPresentModeKHR VSwapchain::getSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
  /** intel says mailbox is better option for games
    * https://software.intel.com/content/www/us/en/develop/articles/api-without-secrets-introduction-to-vulkan-part-2.html
    **/
  std::initializer_list<VkPresentModeKHR> modes={
    VK_PRESENT_MODE_MAILBOX_KHR,      // vsync; wait until vsync
    VK_PRESENT_MODE_FIFO_RELAXED_KHR, // vsync; optimal, but tearing
    VK_PRESENT_MODE_FIFO_KHR,         // vsync; optimal
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

VkExtent2D VSwapchain::getSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                     uint32_t w,uint32_t h) {
  if(capabilities.currentExtent.width!=std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
    }

  VkExtent2D actualExtent = {
    static_cast<uint32_t>(w),
    static_cast<uint32_t>(h)
    };

  actualExtent.width  = std::max(capabilities.minImageExtent.width,
                                 std::min(capabilities.maxImageExtent.width, actualExtent.width));
  actualExtent.height = std::max(capabilities.minImageExtent.height,
                                 std::min(capabilities.maxImageExtent.height, actualExtent.height));

  return actualExtent;
  }

uint32_t VSwapchain::getImageCount(const SwapChainSupport& support) const {
  const uint32_t maxImages=support.capabilities.maxImageCount==0 ? uint32_t(-1) : support.capabilities.maxImageCount;
  uint32_t imageCount=support.capabilities.minImageCount+1;
  if(0<support.capabilities.maxImageCount && imageCount>maxImages)
    imageCount = support.capabilities.maxImageCount;
  return imageCount;
  }

void VSwapchain::aquireNextImage() {
  auto&    slot = sync[syncIndex];
  auto&    f    = fence.aquire[syncIndex];

  vkWaitForFences(device.device.impl,1,&f,VK_TRUE,std::numeric_limits<uint64_t>::max());
  vkResetFences(device.device.impl,1,&f);

  uint32_t id   = uint32_t(-1);
  VkResult code = vkAcquireNextImageKHR(device.device.impl,
                                        swapChain,
                                        std::numeric_limits<uint64_t>::max(),
                                        slot.aquire,
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
  syncIndex = (syncIndex+1)%uint32_t(sync.size());
  }

uint32_t VSwapchain::currentBackBufferIndex() {
  return imgIndex;
  }

void VSwapchain::present(VDevice& dev) {
  auto&    slot = sync[imgIndex];
  auto&    f    = fence.present[imgIndex];
  vkResetFences(device.device.impl,1,&f);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = &slot.present;
  dev.graphicsQueue->submit(1, &submitInfo, f);

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = &slot.present;
  presentInfo.swapchainCount     = 1;
  presentInfo.pSwapchains        = &swapChain;
  presentInfo.pImageIndices      = &imgIndex;

  //auto t = Application::tickCount();
  VkResult code = dev.presentQueue->present(presentInfo);
  if(code==VK_ERROR_OUT_OF_DATE_KHR || code==VK_SUBOPTIMAL_KHR)
    throw SwapchainSuboptimal();
  //Log::i("vkQueuePresentKHR = ",Application::tickCount()-t);
  Detail::vkAssert(code);

  aquireNextImage();
  }

#endif

