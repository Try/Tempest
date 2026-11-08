#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "gapi/vulkan/vtexture.h"
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

    static bool checkPresentationSupport(VkPhysicalDevice device, uint32_t queueFamilyIndex);

    struct SwapChainSupport final {
      VkSurfaceCapabilitiesKHR        capabilities={};
      std::vector<VkSurfaceFormatKHR> formats;
      std::vector<VkPresentModeKHR>   presentModes;
      };

    VkFormat                 format() const;
    uint32_t                 w()      const override;
    uint32_t                 h()      const override;

    void                     reset() override;
    uint32_t                 imageCount() const override;

    uint32_t                 currentBackBufferIndex() override;
    auto                     currentBackBuffer() -> AbstractGraphicsApi::Texture* override;
    void                     present();

    VkImage                  image(size_t i) const;
    VkImageView              view (size_t i) const;

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

    class ImageList {
      public:
        ImageList() = default;
        ImageList(VDevice& dev, VkSwapchainKHR swapChain, VkFormat format);
        ImageList(ImageList&& oth);
        ImageList& operator = (ImageList&& oth);
        ~ImageList();

        uint32_t size() const { return uint32_t(images.size()); }

        std::vector<VkImageView> views;
        std::vector<VkImage>     images;

      private:
        void cleanup();
        VDevice*                 device = {};
      };

    FenceList                aquireFence;
    SemaphoreList            aquireSem;
    FenceList                presentFence;
    SemaphoreList            presentSem;
    ImageList                imageList;

    VDevice&                 device;
    SystemApi::Window*       hwnd      = nullptr;
    VkSurfaceKHR             surface   = VK_NULL_HANDLE;
    VkSwapchainKHR           swapChain = VK_NULL_HANDLE;

    uint32_t                 imgIndex = 0;
    uint32_t                 frameId  = 0;
    VTextureSwapchain        currentAtt;

    VkFormat                 swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D               swapChainExtent = {};
    VkSurfaceCapabilitiesKHR swapChaincurrentCaps = {};

    void                     cleanupSwapchain() noexcept;
    void                     cleanupSurface() noexcept;
    void                     cleanup() noexcept;

    static VkResult          createSurface(VkInstance instance, void* hwnd, VkSurfaceKHR* pSurface);
    VkSurfaceKHR             createSurface(VkInstance instance, void* hwnd);
    void                     createSwapchain(VDevice& device);
    VkResult                 createSwapchain(VDevice& device, const SwapChainSupport& support, const Rect& rect);

    VkSurfaceFormatKHR       findSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR         findSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkCompositeAlphaFlagBitsKHR findAlphaMode(VkCompositeAlphaFlagsKHR supported) const;
    VkExtent2D               findSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, uint32_t w, uint32_t h) const;
    uint32_t                 findImageCount(const SwapChainSupport& support) const;

    bool                     isSwapchainLost(VkResult code) const;
    VkResult                 implAcquireNextImage();
    void                     acquireNextImage();
  };

inline VkFormat VSwapchain::format() const          { return swapChainImageFormat;   }

inline uint32_t VSwapchain::w() const { return swapChainExtent.width;  }

inline uint32_t VSwapchain::h() const { return swapChainExtent.height; }

inline uint32_t VSwapchain::imageCount() const { return uint32_t(imageList.views.size()); }

inline VkImage VSwapchain::image(size_t i) const { return imageList.images[i]; }

inline VkImageView VSwapchain::view(size_t i) const { return imageList.views[i];  }

}}
