#pragma once

#include "vimage.h"

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDevice;

class VSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    VSwapchain()=default;
    VSwapchain(VDevice& device,uint32_t w,uint32_t h);
    VSwapchain(VSwapchain&& other);
    ~VSwapchain();

    void operator=(VSwapchain&& other);

    VkFormat                 format() const { return swapChainImageFormat;   }
    uint32_t                 width()  const { return swapChainExtent.width;  }
    uint32_t                 height() const { return swapChainExtent.height; }

    uint32_t                   imageCount() const { return uint32_t(swapChainImageViews.size()); }

    VkSwapchainKHR           swapChain=VK_NULL_HANDLE;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VImage>      images;

  private:
    VkDevice                 device=nullptr;
    VkFormat                 swapChainImageFormat=VK_FORMAT_UNDEFINED;
    VkExtent2D               swapChainExtent={};

    std::vector<VkImage>     swapChainImages;

    VkSurfaceFormatKHR       chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR         chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D               chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, uint32_t w, uint32_t h);

    void                     createImageViews(VDevice &device);
  };

}}
