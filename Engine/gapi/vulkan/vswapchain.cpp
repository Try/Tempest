#include "vswapchain.h"

#include "vdevice.h"

using namespace Tempest::Detail;

VSwapchain::VSwapchain(VDevice &device, uint32_t w, uint32_t h)
  :device(device.device) {
  auto swapChainSupport = device.querySwapChainSupport();

  VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR   presentMode   = chooseSwapPresentMode  (swapChainSupport.presentModes);
  VkExtent2D         extent        = chooseSwapExtent       (swapChainSupport.capabilities,w,h);

  uint32_t imageCount=swapChainSupport.capabilities.minImageCount+1;
  if(swapChainSupport.capabilities.maxImageCount>0 && imageCount>swapChainSupport.capabilities.maxImageCount)
    imageCount = swapChainSupport.capabilities.maxImageCount;

  auto indices = device.findQueueFamilies();
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

  VkSwapchainCreateInfoKHR createInfo = {};
  createInfo.sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = device.surface;

  createInfo.minImageCount    = imageCount;
  createInfo.imageFormat      = surfaceFormat.format;
  createInfo.imageColorSpace  = surfaceFormat.colorSpace;
  createInfo.imageExtent      = extent;
  createInfo.imageArrayLayers = 1;
  //createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if( indices.graphicsFamily!=indices.presentFamily ) {
    createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

  createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode    = presentMode;
  createInfo.clipped        = VK_TRUE;

  if(vkCreateSwapchainKHR(device.device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
    throw std::runtime_error("failed to create swap chain!");

  vkGetSwapchainImagesKHR(device.device, swapChain, &imageCount, nullptr);
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(device.device, swapChain, &imageCount, swapChainImages.data());

  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent      = extent;

  createImageViews(device);
  images.resize(swapChainImageViews.size());
  for(size_t i=0;i<images.size();++i) {
    images[i].impl               = swapChainImages[i];
    images[i].device             = device.device;
    images[i].presentQueueFamily = indices.presentFamily;
    }
  }

VSwapchain::VSwapchain(VSwapchain &&other) {
  std::swap(swapChainImages,other.swapChainImages);
  std::swap(swapChainImageViews,other.swapChainImageViews);
  std::swap(swapChain,other.swapChain);
  std::swap(swapChainExtent,other.swapChainExtent);
  std::swap(swapChainImageFormat,other.swapChainImageFormat);
  std::swap(device,other.device);
  }

VSwapchain::~VSwapchain() {
  if(device==nullptr)
    return;

  for(size_t i=0;i<images.size();++i)
    images[i].device = nullptr;

  vkDeviceWaitIdle(device);

  for(auto imageView : swapChainImageViews)
    if(imageView!=VK_NULL_HANDLE)
      vkDestroyImageView(device,imageView,nullptr);

  if(swapChain!=VK_NULL_HANDLE)
    vkDestroySwapchainKHR(device,swapChain,nullptr);
  }

void VSwapchain::operator=(VSwapchain &&other) {
  std::swap(swapChainImages,other.swapChainImages);
  std::swap(swapChainImageViews,other.swapChainImageViews);
  std::swap(swapChain,other.swapChain);
  std::swap(swapChainExtent,other.swapChainExtent);
  std::swap(swapChainImageFormat,other.swapChainImageFormat);
  std::swap(device,other.device);
  }

void VSwapchain::createImageViews(VDevice &device) {
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

    if(vkCreateImageView(device.device,&createInfo,nullptr,&swapChainImageViews[i])!=VK_SUCCESS) {
      throw std::runtime_error("failed to create image views!");
      }
    }
  }

VkSurfaceFormatKHR VSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
  if(availableFormats.size()==1 && availableFormats[0].format==VK_FORMAT_UNDEFINED)
    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  for(const auto& availableFormat : availableFormats) {
    if(availableFormat.format==VK_FORMAT_B8G8R8A8_UNORM &&
       availableFormat.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return availableFormat;
    }

  return availableFormats[0];
  }


VkPresentModeKHR VSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
  std::initializer_list<VkPresentModeKHR> modes={
    VK_PRESENT_MODE_FIFO_KHR,         // vsync; optimal
    VK_PRESENT_MODE_FIFO_RELAXED_KHR, // vsync; optimal, but tearing
    VK_PRESENT_MODE_IMMEDIATE_KHR,    // no vsync
    VK_PRESENT_MODE_MAILBOX_KHR       // vsync; wait until vsync
    };

  for(auto mode:modes){
    for(const auto available:availablePresentModes)
      if(available==mode)
        return mode;
    }

  // no good modes
  return availablePresentModes[0];
  }

VkExtent2D VSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
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
