#include "vswapchain.h"

#include <Tempest/SystemApi>

#include "vdevice.h"
#include "vsemaphore.h"

using namespace Tempest;
using namespace Tempest::Detail;

VSwapchain::VSwapchain(VDevice &device, SystemApi::Window* hwnd)
  :device(device), hwnd(hwnd) {
  try {
    surface = device.createSurface(hwnd);

    auto     support  = device.querySwapChainSupport(surface);
    uint32_t imgCount = getImageCount(support);
    createSwapchain(device,support,hwnd,imgCount);
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
  for(auto imageView : swapChainImageViews)
    if(imageView!=VK_NULL_HANDLE)
      vkDestroyImageView(device.device,imageView,nullptr);
  swapChainImageViews.clear();
  swapChainImages.clear();

  if(swapChain!=VK_NULL_HANDLE)
    vkDestroySwapchainKHR(device.device,swapChain,nullptr);

  swapChain            = VK_NULL_HANDLE;
  swapChainImageFormat = VK_FORMAT_UNDEFINED;
  swapChainExtent      = {};
  }

void VSwapchain::cleanupSurface() noexcept {
  if(surface!=VK_NULL_HANDLE)
    vkDestroySurfaceKHR(device.instance,surface,nullptr);
  }

void VSwapchain::reset() {
  cleanupSwapchain();

  try {
    auto     support  = device.querySwapChainSupport(surface);
    uint32_t imgCount = getImageCount(support);
    createSwapchain(device,support,hwnd,imgCount);
    }
  catch(...) {
    cleanup();
    throw;
    }
  }

void VSwapchain::cleanup() noexcept {
  vkDeviceWaitIdle(device.device);

  cleanupSwapchain();
  cleanupSurface();
  }

void VSwapchain::createSwapchain(VDevice& device, const VDevice::SwapChainSupport& swapChainSupport, SystemApi::Window* hwnd, uint32_t imgCount) {
  auto& indices          = device.props;
  const uint32_t qidx[]  = {device.props.presentFamily,device.props.graphicsFamily};

  VkBool32 support=false;
  vkGetPhysicalDeviceSurfaceSupportKHR(device.physicalDevice,indices.presentFamily,surface,&support);
  if(!support)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  const Rect         rect          = SystemApi::windowClientRect(hwnd);
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

  if( indices.graphicsFamily!=indices.presentFamily ) {
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

  if(vkCreateSwapchainKHR(device.device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent      = extent;

  createImageViews(device);
  }

void VSwapchain::createImageViews(VDevice &device) {
  uint32_t imgCount=0;
  vkGetSwapchainImagesKHR(device.device, swapChain, &imgCount, nullptr);
  swapChainImages.resize(imgCount);
  vkGetSwapchainImagesKHR(device.device, swapChain, &imgCount, swapChainImages.data());

  swapChainImageViews.resize(swapChainImages.size());

  for(size_t i=0; i<swapChainImages.size(); i++) {
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = swapChainImages[i];
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

    if(vkCreateImageView(device.device,&createInfo,nullptr,&swapChainImageViews[i])!=VK_SUCCESS)
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
    * https://www.reddit.com/r/vulkan/comments/7xm0n4/screentearing_with_vk_present_mode_mailbox_khr/
    **/
  std::initializer_list<VkPresentModeKHR> modes={
    VK_PRESENT_MODE_MAILBOX_KHR,      // vsync; wait until vsync
    VK_PRESENT_MODE_FIFO_KHR,         // vsync; optimal
    VK_PRESENT_MODE_FIFO_RELAXED_KHR, // vsync; optimal, but tearing
    VK_PRESENT_MODE_IMMEDIATE_KHR,    // no vsync
    };

  for(auto mode:modes){
    for(const auto available:availablePresentModes)
      if(available==mode)
        return mode;
    }

  // no good modes
  return availablePresentModes[0];
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

uint32_t VSwapchain::getImageCount(const VDevice::SwapChainSupport& support) const {
  const uint32_t maxImages=support.capabilities.maxImageCount==0 ? uint32_t(-1) : support.capabilities.maxImageCount;
  uint32_t imageCount=support.capabilities.minImageCount+1;
  if(support.capabilities.maxImageCount>0 && imageCount>maxImages)
    imageCount = support.capabilities.maxImageCount;
  return imageCount;
  }

uint32_t VSwapchain::nextImage(AbstractGraphicsApi::Semaphore* onReady) {
  Detail::VSemaphore* rx=reinterpret_cast<Detail::VSemaphore*>(onReady);

  uint32_t id   = uint32_t(-1);
  VkResult code = vkAcquireNextImageKHR(device.device,
                                        swapChain,
                                        std::numeric_limits<uint64_t>::max(),
                                        rx->impl,
                                        VK_NULL_HANDLE,
                                        &id);
  if(code==VK_ERROR_OUT_OF_DATE_KHR)
    throw DeviceLostException();

  if(code!=VK_SUCCESS && code!=VK_SUBOPTIMAL_KHR)
    throw std::runtime_error("failed to acquire swap chain image!");
  return id;
  }
