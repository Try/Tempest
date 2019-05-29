#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "vallocator.h"
#include "vcommandbuffer.h"
#include "vcommandpool.h"
#include "vfence.h"
#include "vulkanapi_impl.h"
#include "exceptions/exception.h"

namespace Tempest {
namespace Detail{

class VSwapchain;
class VCommandBuffer;

class VFence;
class VSemaphore;

class VTexture;

inline void vkAssert(VkResult code){
  switch( code ) {
    case VkResult::VK_SUCCESS:
      return;
    case VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY:
      throw std::system_error(Tempest::GraphicsErrc::OutOfVideoMemory);
    case VkResult::VK_ERROR_OUT_OF_HOST_MEMORY:
      //throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
      throw std::bad_alloc();
    case VkResult::VK_ERROR_DEVICE_LOST:
      throw DeviceLostException();

    default:
      throw std::runtime_error("engine internal error"); //TODO
    }
  }

inline VkFormat nativeFormat(TextureFormat f){
  static const VkFormat vfrm[]={
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R8_UNORM,
    VK_FORMAT_R8G8B8_UNORM,
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R16G16_UNORM,
    VK_FORMAT_D16_UNORM,
    VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
    VK_FORMAT_BC2_UNORM_BLOCK,
    VK_FORMAT_BC3_UNORM_BLOCK,
    };
  return vfrm[f];
  }

inline VkSamplerAddressMode nativeFormat(ClampMode f){
  static const VkSamplerAddressMode vfrm[]={
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_REPEAT,
    };
  return vfrm[int(f)];
  }

class VDevice : public AbstractGraphicsApi::Device {
  public:
    struct QueueFamilyIndices {
      uint32_t graphicsFamily=uint32_t(-1);
      uint32_t presentFamily =uint32_t(-1);

      bool isComplete() {
        return graphicsFamily!=std::numeric_limits<uint32_t>::max() &&
               presentFamily !=std::numeric_limits<uint32_t>::max();
        }
      };

    struct SwapChainSupportDetails {
      VkSurfaceCapabilitiesKHR        capabilities={};
      std::vector<VkSurfaceFormatKHR> formats;
      std::vector<VkPresentModeKHR>   presentModes;
      };

    using ResPtr = Detail::DSharedPtr<AbstractGraphicsApi::Shared*>;
    using BufPtr = Detail::DSharedPtr<VBuffer*>;
    using TexPtr = Detail::DSharedPtr<VTexture*>;

    VDevice(VulkanApi& api,void* hwnd);
    ~VDevice();

    VkSurfaceKHR            surface       =VK_NULL_HANDLE;
    VkPhysicalDevice        physicalDevice=nullptr;
    VkDevice                device        =nullptr;

    std::mutex              graphicsSync;
    VkQueue                 graphicsQueue =nullptr;
    VkQueue                 presentQueue  =nullptr;

    VAllocator              allocator;
    AbstractGraphicsApi::Caps caps;
    std::mutex              allocSync;

    VkResult                nextImg(VSwapchain& sw,uint32_t& imageId,VSemaphore& onReady);
    VkResult                present(VSwapchain& sw,const VSemaphore *wait,size_t wSize,uint32_t imageId);

    void                    waitData();

    SwapChainSupportDetails querySwapChainSupport() { return querySwapChainSupport(physicalDevice); }
    QueueFamilyIndices      findQueueFamilies    () { return findQueueFamilies(physicalDevice);     }
    uint32_t                memoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags props) const;

    void                    getCaps(AbstractGraphicsApi::Caps& c);
    void                    submitQueue(VkQueue q, VkSubmitInfo& info, VkFence fence, bool waitData);

    class Data final {
      public:
        Data(VDevice& dev);
        ~Data();

        void start();
        void flush(const Detail::VBuffer& src, size_t size);
        void copy(Detail::VBuffer&  dest, const Detail::VBuffer& src, size_t size);
        void copy(Detail::VTexture& dest, uint32_t w, uint32_t h, uint32_t mip, const Detail::VBuffer& src, size_t offset);

        void changeLayout(Detail::VTexture& dest, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount);
        void generateMipmap(VTexture& image, VkFormat frm, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels);

        void hold(BufPtr& b);
        void hold(TexPtr& b);
        void commit();

      private:
        VDevice&                    dev;
        std::lock_guard<std::mutex> sync;
        bool                        commited=true;
      };

  private:
    class DataHelper {
      public:
        DataHelper(VDevice &owner);
        ~DataHelper();

        void begin();
        void end();
        void wait();

        VDevice&               owner;
        VCommandPool           cmdPool;
        VCommandBuffer         cmdBuffer;
        std::vector<ResPtr>    hold;

      private:
        VFence                 fence;
        VkQueue                graphicsQueue=nullptr;

        std::mutex             waitSync;
        bool                   hasToWait=false;
      };

    VkInstance             instance;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    std::unique_ptr<DataHelper> data;

    void                    pickPhysicalDevice();
    bool                    isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices      findQueueFamilies(VkPhysicalDevice device);
    bool                    checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    void createSurface(VulkanApi &api,void* hwnd);
    void createLogicalDevice(VulkanApi &api);
  };

}}
