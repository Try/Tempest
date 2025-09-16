#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

namespace Tempest {

namespace Detail {

class VDevice;
class VFramebufferMap;

class VSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    VSwapchain(VDevice& device, SystemApi::Window* hwnd);
    VSwapchain(VSwapchain&& other) = delete;
    ~VSwapchain() override;
    VSwapchain& operator=(VSwapchain&& other) = delete;

    static bool checkPresentSupport(VkPhysicalDevice device, uint32_t queueFamilyIndex);

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

    uint32_t                 currentBackBufferIndex() override;
    void                     present();

    VFramebufferMap*         map = nullptr;

    VkSwapchainKHR           swapChain=VK_NULL_HANDLE;
    std::vector<VkImageView> views;
    std::vector<VkImage>     images;

    enum SyncState : uint8_t {
      S_Idle,
      S_Pending,
      S_Aquired,
      S_Draw,
      };

    struct Sync {
      SyncState   state   = S_Idle;
      uint32_t    imgId   = uint32_t(-1);
      VkSemaphore acquire = VK_NULL_HANDLE;
      };
    std::vector<Sync>        sync;

  private:
    class FenceList {
      public:
        FenceList() = default;
        FenceList(VkDevice dev, uint32_t cnt);
        FenceList(FenceList&& oth);
        FenceList& operator = (FenceList&& oth);
        ~FenceList();

        void waitAll();
        VkFence& operator[](size_t i) { return data[i]; }

      private:
        VkDevice                   dev = VK_NULL_HANDLE;
        std::unique_ptr<VkFence[]> data;
        uint32_t                   size = 0;
      };

    class SemaphoreList {
      public:
        SemaphoreList() = default;
        SemaphoreList(VkDevice dev, uint32_t cnt);
        SemaphoreList(SemaphoreList&& oth);
        SemaphoreList& operator = (SemaphoreList&& oth);
        ~SemaphoreList();

        VkSemaphore& operator[](size_t i) { return data[i]; }

      private:
        VkDevice                       dev = VK_NULL_HANDLE;
        std::unique_ptr<VkSemaphore[]> data;
        uint32_t                       size = 0;
      };

    FenceList                aquireFence;
    SemaphoreList            aquireSem;
    FenceList                presentFence;
    SemaphoreList            presentSem;

    VDevice&                 device;
    SystemApi::Window*       hwnd     = nullptr;
    VkSurfaceKHR             surface  = VK_NULL_HANDLE;

    uint32_t                 imgIndex = 0;
    uint32_t                 frameId  = 0;

    VkFormat                 swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D               swapChainExtent = {};

    void                     cleanupSwapchain() noexcept;
    void                     cleanupSurface() noexcept;
    void                     cleanup() noexcept;

    VkSurfaceKHR             createSurface(VkInstance instance, void* hwnd);
    void                     createSwapchain(VDevice& device);
    VkResult                 createSwapchain(VDevice& device, const SwapChainSupport& support, const Rect& rect, uint32_t imgCount);
    void                     createImageViews(VDevice &device);

    VkSurfaceFormatKHR       findSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR         findSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D               findSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, uint32_t w, uint32_t h);
    uint32_t                 findImageCount(const SwapChainSupport& support) const;

    VkResult                 implAcquireNextImage();
    void                     acquireNextImage();
  };

}}
