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
#include "utility/spinlock.h"
#include "utility/compiller_hints.h"

namespace Tempest {
namespace Detail {

class VSwapchain;
class VCommandBuffer;

class VFence;
class VSemaphore;

class VTexture;

inline void vkAssert(VkResult code){
  if(T_LIKELY(code==VkResult::VK_SUCCESS))
    return;

  switch( code ) {
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
    VK_FORMAT_X8_D24_UNORM_PACK32,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
    VK_FORMAT_BC2_UNORM_BLOCK,
    VK_FORMAT_BC3_UNORM_BLOCK,
    };
  return vfrm[f];
  }

inline bool nativeIsDepthFormat(VkFormat f){
  return f==VK_FORMAT_D16_UNORM ||
         f==VK_FORMAT_X8_D24_UNORM_PACK32 ||
         f==VK_FORMAT_D32_SFLOAT ||
         f==VK_FORMAT_S8_UINT ||
         f==VK_FORMAT_D16_UNORM_S8_UINT ||
         f==VK_FORMAT_D24_UNORM_S8_UINT ||
         f==VK_FORMAT_D32_SFLOAT_S8_UINT;
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
  private:
    class DataStream;
    class FakeWindow;

  public:
    using ResPtr = Detail::DSharedPtr<AbstractGraphicsApi::Shared*>;
    using BufPtr = Detail::DSharedPtr<VBuffer*>;
    using TexPtr = Detail::DSharedPtr<VTexture*>;

    using VkProps = Detail::VulkanApi::VkProp;

    struct SwapChainSupport final {
      VkSurfaceCapabilitiesKHR        capabilities={};
      std::vector<VkSurfaceFormatKHR> formats;
      std::vector<VkPresentModeKHR>   presentModes;
      };

    VDevice(VulkanApi& api,const char* gpuName);
    ~VDevice() override;

    struct Queue final {
      std::mutex sync;
      VkQueue    impl=nullptr;
      uint32_t   family=0;

      void       submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
      VkResult   present(VkPresentInfoKHR& presentInfo);
      };

    struct MemIndex final {
      uint32_t headId=0;
      uint32_t typeId=0;
      };

    VkInstance              instance           =nullptr;
    VkPhysicalDevice        physicalDevice     =nullptr;
    VkDevice                device             =nullptr;

    Queue                   queues[3];
    Queue*                  graphicsQueue=nullptr;
    Queue*                  presentQueue =nullptr;

    std::mutex              allocSync;
    VAllocator              allocator;

    VkProps                 props={};

    PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2 = nullptr;
    PFN_vkGetImageMemoryRequirements2KHR  vkGetImageMemoryRequirements2  = nullptr;
    std::unique_ptr<VCommandPool> cmdMain;

    VkResult                present(VSwapchain& sw,const VSemaphore *wait,size_t wSize,uint32_t imageId);

    void                    waitData();
    const char*             renderer() const override;
    void                    waitIdle() const override;

    VkSurfaceKHR            createSurface(void* hwnd);
    SwapChainSupport        querySwapChainSupport(VkSurfaceKHR surface) { return querySwapChainSupport(physicalDevice,surface); }
    MemIndex                memoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags props, VkImageTiling tiling) const;

    class Data final {
      public:
        Data(VDevice& dev);
        ~Data() noexcept(false);

        void flush(const Detail::VBuffer& src, size_t size);
        void copy(Detail::VBuffer&  dest, const Detail::VBuffer& src, size_t size);
        void copy(Detail::VTexture& dest, uint32_t w, uint32_t h, uint32_t mip, const Detail::VBuffer&  src, size_t offset);
        void copy(Detail::VBuffer&  dest, uint32_t w, uint32_t h, uint32_t mip, const Detail::VTexture& src, size_t offset);

        void changeLayout(Detail::VTexture& dest, VkFormat frm, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount);
        void generateMipmap(VTexture& image, VkFormat frm, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels);

        void hold(BufPtr& b);
        void hold(TexPtr& b);
        void commit();

      private:
        std::lock_guard<std::mutex> sync;
        DataStream&                 stream;
        VkPhysicalDevice            physicalDevice=VK_NULL_HANDLE;
        bool                        commited=true;
      };

  private:
    enum  DataState : uint8_t {
      StIdle      = 0,
      StRecording = 1,
      StWait      = 2,
      };

    class DataStream {
      public:
        DataStream(VDevice &owner);
        ~DataStream();

        void begin();
        void end();
        void wait();

        VDevice&               owner;
        VCommandPool           cmdPool;
        VCommandBuffer         cmdBuffer;
        std::vector<ResPtr>    hold;

        VFence                 fence;
        Queue*                 gpuQueue=nullptr;
        std::atomic<DataState> state{StIdle};
      };

    struct DataMgr {
      DataMgr(VDevice& dev);
      ~DataMgr();

      DataStream& get();
      void        wait();

      static constexpr size_t     size=2;
      SpinLock                    sync[size];
      std::unique_ptr<DataStream> streams[size];
      std::atomic_int             at{0};
      };

    VkPhysicalDeviceMemoryProperties memoryProperties;
    std::unique_ptr<DataMgr>         data;

    void                    implInit(VkPhysicalDevice pdev, VkSurfaceKHR surf);
    void                    pickPhysicalDevice();
    bool                    isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surf, const char* gpuName);
    void                    deviceQueueProps(VulkanApi::VkProp& prop, VkPhysicalDevice device, VkSurfaceKHR surf);
    bool                    checkDeviceExtensionSupport(VkPhysicalDevice device);
    auto                    extensionsList(VkPhysicalDevice device) -> std::vector<VkExtensionProperties>;
    bool                    checkForExt(const std::vector<VkExtensionProperties>& list,const char* name);
    SwapChainSupport        querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    void                    createLogicalDevice(VkPhysicalDevice pdev);
  };

}}
