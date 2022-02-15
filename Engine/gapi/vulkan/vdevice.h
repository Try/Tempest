#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
#include <stdexcept>
#include "vulkan_sdk.h"

#include "vallocator.h"
#include "vcommandbuffer.h"
#include "vcommandpool.h"
#include "vswapchain.h"
#include "vfence.h"
#include "vulkanapi_impl.h"
#include "vframebuffermap.h"
#include "exceptions/exception.h"
#include "utility/spinlock.h"
#include "utility/compiller_hints.h"
#include "gapi/uploadengine.h"

namespace Tempest {
namespace Detail {

class VFence;

class VTexture;

inline void vkAssert(VkResult code){
  if(T_LIKELY(code==VkResult::VK_SUCCESS))
    return;

  switch( code ) {
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      throw std::system_error(Tempest::GraphicsErrc::OutOfVideoMemory);
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      //throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
      throw std::bad_alloc();
    case VK_ERROR_DEVICE_LOST:
      throw DeviceLostException();

    default:
      throw std::runtime_error("engine internal error"); //TODO
    }
  }

inline VkFormat nativeFormat(TextureFormat f) {
  static const VkFormat vfrm[]={
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R8_UNORM,
    VK_FORMAT_R8G8_UNORM,
    VK_FORMAT_R8G8B8_UNORM,
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R16_UNORM,
    VK_FORMAT_R16G16_UNORM,
    VK_FORMAT_R16G16B16_UNORM,
    VK_FORMAT_R16G16B16A16_UNORM,
    VK_FORMAT_R32_SFLOAT,
    VK_FORMAT_R32G32_SFLOAT,
    VK_FORMAT_R32G32B32_SFLOAT,
    VK_FORMAT_R32G32B32A32_SFLOAT,
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

inline VkBlendFactor nativeFormat(RenderState::BlendMode b) {
  switch(b) {
    case RenderState::BlendMode::Zero:             return VK_BLEND_FACTOR_ZERO;
    case RenderState::BlendMode::One:              return VK_BLEND_FACTOR_ONE;
    case RenderState::BlendMode::SrcColor:         return VK_BLEND_FACTOR_SRC_COLOR;
    case RenderState::BlendMode::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case RenderState::BlendMode::SrcAlpha:         return VK_BLEND_FACTOR_SRC_ALPHA;
    case RenderState::BlendMode::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case RenderState::BlendMode::DstAlpha:         return VK_BLEND_FACTOR_DST_ALPHA;
    case RenderState::BlendMode::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case RenderState::BlendMode::DstColor:         return VK_BLEND_FACTOR_DST_COLOR;
    case RenderState::BlendMode::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case RenderState::BlendMode::SrcAlphaSaturate: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    }
  return VK_BLEND_FACTOR_ZERO;
  }

inline VkBlendOp nativeFormat(RenderState::BlendOp op) {
  switch(op) {
    case RenderState::BlendOp::Add:             return VK_BLEND_OP_ADD;
    case RenderState::BlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
    case RenderState::BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
    case RenderState::BlendOp::Min:             return VK_BLEND_OP_MIN;
    case RenderState::BlendOp::Max:             return VK_BLEND_OP_MAX;
    }
  return VK_BLEND_OP_ADD;
  }

inline VkCullModeFlags nativeFormat(RenderState::CullMode c) {
  switch(c) {
    case RenderState::CullMode::Back:   return VK_CULL_MODE_BACK_BIT;
    case RenderState::CullMode::Front:  return VK_CULL_MODE_FRONT_BIT;
    case RenderState::CullMode::NoCull: return 0;
    }
  return 0;
  }

inline VkCompareOp nativeFormat(RenderState::ZTestMode zm) {
  switch(zm) {
    case RenderState::ZTestMode::Always:  return VK_COMPARE_OP_ALWAYS;
    case RenderState::ZTestMode::Never:   return VK_COMPARE_OP_NEVER;
    case RenderState::ZTestMode::Greater: return VK_COMPARE_OP_GREATER;
    case RenderState::ZTestMode::Less:    return VK_COMPARE_OP_LESS;
    case RenderState::ZTestMode::GEqual:  return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case RenderState::ZTestMode::LEqual:  return VK_COMPARE_OP_LESS_OR_EQUAL;
    case RenderState::ZTestMode::NOEqual: return VK_COMPARE_OP_NOT_EQUAL;
    case RenderState::ZTestMode::Equal:   return VK_COMPARE_OP_EQUAL;
    }
  return VK_COMPARE_OP_ALWAYS;
  }

inline VkFilter nativeFormat(Filter f){
  static const VkFilter vfrm[]={
    VK_FILTER_NEAREST,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR
    };
  return vfrm[int(f)];
  }

inline VkFormat nativeFormat(Decl::ComponentType t) {
  switch(t) {
    case Decl::float0:
    case Decl::count:
      return VK_FORMAT_UNDEFINED;
    case Decl::float1:
      return VK_FORMAT_R32_SFLOAT;
    case Decl::float2:
      return VK_FORMAT_R32G32_SFLOAT;
    case Decl::float3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case Decl::float4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Decl::int1:
      return VK_FORMAT_R32_SINT;
    case Decl::int2:
      return VK_FORMAT_R32G32_SINT;
    case Decl::int3:
      return VK_FORMAT_R32G32B32_SINT;
    case Decl::int4:
      return VK_FORMAT_R32G32B32A32_SINT;
    case Decl::uint1:
      return VK_FORMAT_R32_UINT;
    case Decl::uint2:
      return VK_FORMAT_R32G32_UINT;
    case Decl::uint3:
      return VK_FORMAT_R32G32B32_UINT;
    case Decl::uint4:
      return VK_FORMAT_R32G32B32A32_UINT;
    }
  return VK_FORMAT_UNDEFINED;
  }

inline VkIndexType nativeFormat(Detail::IndexClass icls) {
  switch(icls) {
    case Detail::IndexClass::i16:
      return VK_INDEX_TYPE_UINT16;
    case Detail::IndexClass::i32:
      return VK_INDEX_TYPE_UINT32;
    }
  return VK_INDEX_TYPE_UINT16;
  }

class VDevice : public AbstractGraphicsApi::Device {
  private:
    class DataStream;

  public:
    using ResPtr = Detail::DSharedPtr<AbstractGraphicsApi::Shared*>;
    using BufPtr = Detail::DSharedPtr<VBuffer*>;
    using TexPtr = Detail::DSharedPtr<VTexture*>;

    using VkProps = Detail::VulkanInstance::VkProp;

    using SwapChainSupport = VSwapchain::SwapChainSupport;

    VDevice(VulkanInstance& api,const char* gpuName);
    ~VDevice() override;

    struct autoDevice {
      VkDevice impl = nullptr;
      ~autoDevice();
      };

    struct Queue final {
      std::mutex sync;
      VkQueue    impl=nullptr;
      uint32_t   family=0;

      void       submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
      void       submit(uint32_t submitCount, const VkSubmitInfo2KHR* pSubmits, VkFence fence, PFN_vkQueueSubmit2KHR fn);
      VkResult   present(VkPresentInfoKHR& presentInfo);
      void       waitIdle();
      };

    struct MemIndex final {
      uint32_t heapId      = 0;
      uint32_t typeId      = 0;
      bool     hostVisible = false;
      };

    VkInstance              instance           =nullptr;
    VkPhysicalDevice        physicalDevice     =nullptr;
    autoDevice              device;

    Queue                   queues[3];
    Queue*                  graphicsQueue=nullptr;
    Queue*                  presentQueue =nullptr;

    std::mutex              allocSync;
    VAllocator              allocator;

    VFramebufferMap         fboMap;

    VkProps                 props={};

    PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2 = nullptr;
    PFN_vkGetImageMemoryRequirements2KHR  vkGetImageMemoryRequirements2  = nullptr;

    PFN_vkCmdPipelineBarrier2KHR          vkCmdPipelineBarrier2          = nullptr;
    PFN_vkQueueSubmit2KHR                 vkQueueSubmit2                 = nullptr;

    PFN_vkCmdBeginRenderingKHR            vkCmdBeginRenderingKHR         = nullptr;
    PFN_vkCmdEndRenderingKHR              vkCmdEndRenderingKHR           = nullptr;

    PFN_vkGetBufferDeviceAddressKHR       vkGetBufferDeviceAddress       = nullptr;

    PFN_vkCreateAccelerationStructureKHR        vkCreateAccelerationStructure        = nullptr;
    PFN_vkDestroyAccelerationStructureKHR       vkDestroyAccelerationStructure       = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizes = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR     vkCmdBuildAccelerationStructures     = nullptr;

    void                    waitIdle() override;

    void                    submit(VCommandBuffer& cmd, VFence& sync);

    VkSurfaceKHR            createSurface(void* hwnd);
    SwapChainSupport        querySwapChainSupport(VkSurfaceKHR surface) { return querySwapChainSupport(physicalDevice,surface); }
    MemIndex                memoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags props, VkImageTiling tiling) const;

    using DataMgr = UploadEngine<VDevice,VCommandBuffer,VFence,VBuffer>;
    DataMgr&                dataMgr() const { return *data; }

  private:
    VkPhysicalDeviceMemoryProperties memoryProperties;
    std::unique_ptr<DataMgr>         data;
    void                    waitIdleSync(Queue* q, size_t n);

    void                    implInit(VulkanInstance& api, VkPhysicalDevice pdev);
    void                    pickPhysicalDevice();
    bool                    isDeviceSuitable(VkPhysicalDevice device, const char* gpuName);
    void                    deviceQueueProps(VulkanInstance::VkProp& prop, VkPhysicalDevice device);
    bool                    checkDeviceExtensionSupport(VkPhysicalDevice device);
    auto                    extensionsList(VkPhysicalDevice device) -> std::vector<VkExtensionProperties>;
    bool                    checkForExt(const std::vector<VkExtensionProperties>& list,const char* name);
    SwapChainSupport        querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    void                    createLogicalDevice(VulkanInstance& api, VkPhysicalDevice pdev);
  };

}}
