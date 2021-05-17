#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

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

    struct SwapChainSupport final {
      VkSurfaceCapabilitiesKHR        capabilities={};
      std::vector<VkSurfaceFormatKHR> formats;
      std::vector<VkPresentModeKHR>   presentModes;
      };

    VkFormat                 format() const          { return swapChainImageFormat;   }
    uint32_t                 w()      const override { return swapChainExtent.width;  }
    uint32_t                 h()      const override { return swapChainExtent.height; }

    void                     reset() override;
    uint32_t                 imageCount() const override { return uint32_t(views.size()); }
    uint32_t                 nextImage(AbstractGraphicsApi::Semaphore* onReady) override;

    VkSwapchainKHR           swapChain=VK_NULL_HANDLE;
    std::vector<VkImageView> views;
    std::vector<VkImage>     images;

    enum SyncState : uint8_t {
      S_Idle,
      S_Pending,
      S_Draw0,
      S_Draw1,
      };

    struct Sync {
      SyncState   state   = S_Idle;
      uint32_t    imgId   = uint32_t(-1);
      VkSemaphore aquire  = VK_NULL_HANDLE;
      VkSemaphore present = VK_NULL_HANDLE;
      };
    std::vector<Sync> sync;

  private:
    VDevice&                 device;
    SystemApi::Window*       hwnd = nullptr;
    VkSurfaceKHR             surface = VK_NULL_HANDLE;
    uint32_t                 syncIndex = 0;

    VkFormat                 swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D               swapChainExtent={};

    void                     cleanupSwapchain() noexcept;
    void                     cleanupSurface() noexcept;
    void                     cleanup() noexcept;

    void                     createSwapchain(VDevice& device, const SwapChainSupport& support, const Rect& rect, uint32_t imgCount);
    void                     createImageViews(VDevice &device);

    VkSurfaceFormatKHR       getSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR         getSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D               getSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, uint32_t w, uint32_t h);
    uint32_t                 getImageCount(const SwapChainSupport& support) const;

  };

}}
