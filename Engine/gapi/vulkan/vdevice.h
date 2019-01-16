#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "vallocator.h"
#include "vcommandbuffer.h"
#include "vcommandpool.h"
#include "vfence.h"
#include "vulkanapi.h"
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

    VDevice(VulkanApi& api,void* hwnd);
    ~VDevice();

    VkSurfaceKHR            surface       =VK_NULL_HANDLE;
    VkPhysicalDevice        physicalDevice=nullptr;
    VkDevice                device        =nullptr;

    VkQueue                 graphicsQueue =nullptr;
    VkQueue                 presentQueue  =nullptr;

    VAllocator              allocator;
    AbstractGraphicsApi::Caps caps;

    VkResult                nextImg(VSwapchain& sw,uint32_t& imageId,VSemaphore& onReady);
    VkResult                present(VSwapchain& sw,const VSemaphore *wait,size_t wSize,uint32_t imageId);

    void                    draw(VCommandBuffer& cmd, VSemaphore& wait, VSemaphore& onReady, VFence& onReadyCpu);

    void                    copy(Detail::VBuffer&  dest, const Detail::VBuffer& src, size_t size);
    void                    copy(Detail::VTexture& dest, uint32_t w, uint32_t h, const Detail::VBuffer& src);
    void                    changeLayout(Detail::VTexture& dest, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount);

    void                    generateMipmap(VTexture& image, VkFormat frm, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels);
    void                    waitData();

    SwapChainSupportDetails querySwapChainSupport() { return querySwapChainSupport(physicalDevice); }
    QueueFamilyIndices      findQueueFamilies    () { return findQueueFamilies(physicalDevice);     }
    uint32_t                memoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags props) const;

    void                    getCaps(AbstractGraphicsApi::Caps& c);

  private:
    struct DataHelper {
      DataHelper(VDevice &owner);
      VCommandPool           cmdPool;
      VCommandBuffer         cmdBuffer;
      VFence                 fence;
      VkQueue                graphicsQueue=nullptr;
      bool                   firstCommit=true;
      bool                   hasToWait=false;

      void begin();
      void end();
      void wait();
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
