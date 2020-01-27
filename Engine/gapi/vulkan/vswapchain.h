#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

#include "vdevice.h"

namespace Tempest {

class Semaphore;

namespace Detail {

class VDevice;

class VSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    VSwapchain(VDevice& device, SystemApi::Window* hwnd);
    VSwapchain(VSwapchain&& other) = delete;
    ~VSwapchain() override;

    VSwapchain& operator=(VSwapchain&& other) = delete;

    VkFormat                 format() const          { return swapChainImageFormat;   }
    uint32_t                 w()      const override { return swapChainExtent.width;  }
    uint32_t                 h()      const override { return swapChainExtent.height; }

    void                     reset() override;
    uint32_t                 imageCount() const override { return uint32_t(views.size()); }
    uint32_t                 nextImage(AbstractGraphicsApi::Semaphore* onReady) override;

    VkSwapchainKHR           swapChain=VK_NULL_HANDLE;
    std::vector<VkImageView> views;
    std::vector<VkImage>     images;

  private:
    VDevice&                 device;
    SystemApi::Window*       hwnd = nullptr;
    VkSurfaceKHR             surface = VK_NULL_HANDLE;

    VkFormat                 swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D               swapChainExtent={};

    void                     cleanupSwapchain() noexcept;
    void                     cleanupSurface() noexcept;
    void                     cleanup() noexcept;

    void                     createSwapchain(VDevice& device, const VDevice::SwapChainSupport& support, SystemApi::Window* hwnd, uint32_t imgCount);
    void                     createImageViews(VDevice &device);

    VkSurfaceFormatKHR       getSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR         getSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D               getSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, uint32_t w, uint32_t h);
    uint32_t                 getImageCount(const VDevice::SwapChainSupport& support) const;

  };

}}
