#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
#include <Tempest/AccelerationStructure>
#include <stdexcept>
#include <string>
#include "vulkan_sdk.h"

#include "gapi/vulkan/vallocator.h"
#include "gapi/vulkan/vcommandbuffer.h"
#include "gapi/vulkan/vswapchain.h"
#include "gapi/vulkan/vfence.h"
#include "gapi/vulkan/vframebuffermap.h"
#include "gapi/vulkan/vbuffer.h"
#include "gapi/vulkan/vbindlesscache.h"
#include "gapi/vulkan/vpushdescriptor.h"
#include "gapi/vulkan/vsetlayoutcache.h"
#include "gapi/vulkan/vpoolcache.h"
#include "gapi/vulkan/vpsolayoutcache.h"
#include "gapi/uploadengine.h"
#include "exceptions/exception.h"
#include "utility/compiller_hints.h"
#include "vulkanapi_impl.h"

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
      throw std::runtime_error("Engine internal error. VkResult=" + std::to_string(code));
    }
  }

inline VkFormat nativeFormat(TextureFormat f) {
  switch(f) {
    case TextureFormat::Last:
    case TextureFormat::Undefined:
      return VK_FORMAT_UNDEFINED;
    case TextureFormat::R8:
      return VK_FORMAT_R8_UNORM;
    case TextureFormat::RG8:
      return VK_FORMAT_R8G8_UNORM;
    case TextureFormat::RGB8:
      return VK_FORMAT_R8G8B8_UNORM;
    case TextureFormat::RGBA8:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R16:
      return VK_FORMAT_R16_UNORM;
    case TextureFormat::RG16:
      return VK_FORMAT_R16G16_UNORM;
    case TextureFormat::RGB16:
      return VK_FORMAT_R16G16B16_UNORM;
    case TextureFormat::RGBA16:
      return VK_FORMAT_R16G16B16A16_UNORM;
    case TextureFormat::R32F:
      return VK_FORMAT_R32_SFLOAT;
    case TextureFormat::RG32F:
      return VK_FORMAT_R32G32_SFLOAT;
    case TextureFormat::RGB32F:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case TextureFormat::RGBA32F:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case TextureFormat::R32U:
      return VK_FORMAT_R32_UINT;
    case TextureFormat::RG32U:
      return VK_FORMAT_R32G32_UINT;
    case TextureFormat::RGB32U:
      return VK_FORMAT_R32G32B32_UINT;
    case TextureFormat::RGBA32U:
      return VK_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::Depth16:
      return VK_FORMAT_D16_UNORM;
    case TextureFormat::Depth24x8:
      return VK_FORMAT_X8_D24_UNORM_PACK32;
    case TextureFormat::Depth24S8:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::Depth32F:
      return VK_FORMAT_D32_SFLOAT;
    case TextureFormat::DXT1:
      return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case TextureFormat::DXT3:
      return VK_FORMAT_BC2_UNORM_BLOCK;
    case TextureFormat::DXT5:
      return VK_FORMAT_BC3_UNORM_BLOCK;
    case TextureFormat::R11G11B10UF:
      return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case TextureFormat::RGBA16F:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    }
  return VK_FORMAT_UNDEFINED;
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
    case RenderState::ZTestMode::NoEqual: return VK_COMPARE_OP_NOT_EQUAL;
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

inline VkIndexType nativeFormat(IndexClass icls) {
  switch(icls) {
    case Detail::IndexClass::i16:
      return VK_INDEX_TYPE_UINT16;
    case Detail::IndexClass::i32:
      return VK_INDEX_TYPE_UINT32;
    }
  return VK_INDEX_TYPE_UINT16;
  }

inline VkDescriptorType nativeFormat(ShaderReflection::Class cls) {
  switch(cls) {
    case ShaderReflection::Ubo:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case ShaderReflection::Texture:
      return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case ShaderReflection::Image:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case ShaderReflection::Sampler:
      return VK_DESCRIPTOR_TYPE_SAMPLER;
    case ShaderReflection::SsboR:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderReflection::SsboRW :
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderReflection::ImgR:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case ShaderReflection::ImgRW:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case ShaderReflection::Tlas:
      return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    case ShaderReflection::Push:
    case ShaderReflection::Count:
      return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
  return VK_DESCRIPTOR_TYPE_MAX_ENUM;
  }

inline VkShaderStageFlagBits nativeFormat(ShaderReflection::Stage st) {
  uint32_t stageFlags = 0;
  if(st&ShaderReflection::Compute)
    stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
  if(st&ShaderReflection::Vertex)
    stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
  if(st&ShaderReflection::Control)
    stageFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  if(st&ShaderReflection::Evaluate)
    stageFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  if(st&ShaderReflection::Geometry)
    stageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
  if(st&ShaderReflection::Fragment)
    stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
  if(st&ShaderReflection::Task)
    stageFlags |= VK_SHADER_STAGE_TASK_BIT_EXT;
  if(st&ShaderReflection::Mesh)
    stageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT;
  return VkShaderStageFlagBits(stageFlags);
  }

inline VkPrimitiveTopology nativeFormat(Topology tp) {
  switch(tp) {
    case Topology::Points:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case Topology::Lines:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case Topology::Triangles:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }

inline VkGeometryInstanceFlagsKHR nativeFormat(RtInstanceFlags f) {
  VkGeometryInstanceFlagsKHR ret = 0;
  if((f & RtInstanceFlags::NonOpaque)==RtInstanceFlags::NonOpaque)
    ret |= VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR; else
    ret |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
  if((f & RtInstanceFlags::CullDisable)==RtInstanceFlags::CullDisable)
    ret |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  if((f & RtInstanceFlags::CullFlip)==RtInstanceFlags::CullFlip)
    ret |= VK_GEOMETRY_INSTANCE_TRIANGLE_FLIP_FACING_BIT_KHR;
  return ret;
  }

class VDevice : public AbstractGraphicsApi::Device {
  private:
    class DataStream;

  public:
    using ResPtr = Detail::DSharedPtr<AbstractGraphicsApi::Shared*>;
    using BufPtr = Detail::DSharedPtr<VBuffer*>;
    using TexPtr = Detail::DSharedPtr<VTexture*>;

    using SwapChainSupport = VSwapchain::SwapChainSupport;

    VDevice(VulkanInstance& api, std::string_view gpuName);
    ~VDevice() override;

    struct autoDevice {
      VkDevice impl = nullptr;
      ~autoDevice();
      };

    struct VkProps : Tempest::AbstractGraphicsApi::Props {
      uint32_t graphicsFamily = uint32_t(-1);
      uint32_t presentFamily  = uint32_t(-1);

      size_t   nonCoherentAtomSize = 0;
      size_t   bufferImageGranularity = 0;
      size_t   accelerationStructureScratchOffsetAlignment = 0;

      uint64_t filteredLinearFormat = 0;
      bool     hasFilteredFormat(TextureFormat f) const;

      bool     hasMemRq2          = false;
      bool     hasDedicatedAlloc  = false;
      bool     hasSync2           = false;
      bool     hasDeviceAddress   = false;
      bool     hasDynRendering    = false;
      bool     hasDescIndexing    = false;
      bool     hasBarycentrics    = false;
      bool     hasSpirv_1_4       = false;
      bool     hasDebugMarker     = false;
      bool     hasRobustness2     = false;
      bool     hasStoreOpNone     = false;
      bool     hasMaintenance1    = false;
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

    void                    waitIdle() override;
    void                    submit(VCommandBuffer& cmd, VFence* sync);

    static void             deviceProps(const VulkanInstance& api, VkPhysicalDevice physicalDevice, VkProps& c);
    static void             devicePropsShort(const VulkanInstance& api, VkPhysicalDevice physicalDevice, VkProps& props);

    VkSurfaceKHR            createSurface(void* hwnd);
    SwapChainSupport        querySwapChainSupport(VkSurfaceKHR surface) { return querySwapChainSupport(physicalDevice,surface); }
    MemIndex                memoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags props, VkImageTiling tiling) const;

    using DataMgr = UploadEngine<VDevice,VCommandBuffer,VFence,VBuffer>;
    DataMgr&                dataMgr() const { return *data; }

    VBuffer&                dummySsbo();

    uint32_t                roundUpDescriptorCount(ShaderReflection::Class cls, size_t cnt);
    VkDescriptorSetLayout   bindlessArrayLayout(ShaderReflection::Class cls, size_t cnt);

    VkInstance              instance       = nullptr;
    VkPhysicalDevice        physicalDevice = nullptr;
    autoDevice              device;

    Queue                   queues[3];
    Queue*                  graphicsQueue = nullptr;
    Queue*                  presentQueue  = nullptr;

    std::mutex              allocSync;
    VAllocator              allocator;

    VFramebufferMap         fboMap;
    VSetLayoutCache         setLayouts;
    VPsoLayoutCache         psoLayouts;
    VPoolCache              descPool;
    VBindlessCache          bindless;

    VkProps                 props={};

    PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2 = nullptr;
    PFN_vkGetImageMemoryRequirements2KHR  vkGetImageMemoryRequirements2  = nullptr;

    PFN_vkCmdPipelineBarrier2KHR          vkCmdPipelineBarrier2          = nullptr;
    PFN_vkQueueSubmit2KHR                 vkQueueSubmit2                 = nullptr;

    PFN_vkCmdBeginRenderingKHR            vkCmdBeginRenderingKHR         = nullptr;
    PFN_vkCmdEndRenderingKHR              vkCmdEndRenderingKHR           = nullptr;

    PFN_vkGetBufferDeviceAddressKHR                vkGetBufferDeviceAddress                = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddress = nullptr;

    PFN_vkCreateAccelerationStructureKHR        vkCreateAccelerationStructure        = nullptr;
    PFN_vkDestroyAccelerationStructureKHR       vkDestroyAccelerationStructure       = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizes = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR     vkCmdBuildAccelerationStructures     = nullptr;

    PFN_vkCmdDrawMeshTasksEXT                   vkCmdDrawMeshTasks = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectEXT           vkCmdDrawMeshTasksIndirect = nullptr;

    PFN_vkCmdDebugMarkerBeginEXT                vkCmdDebugMarkerBegin      = nullptr;
    PFN_vkCmdDebugMarkerEndEXT                  vkCmdDebugMarkerEnd        = nullptr;
    PFN_vkDebugMarkerSetObjectNameEXT           vkDebugMarkerSetObjectName = nullptr;

  private:
    VkPhysicalDeviceMemoryProperties memoryProperties;
    std::unique_ptr<DataMgr>         data;

    std::mutex              syncSsbo;
    VBuffer                 dummySsboVal;

    void                    implInit(VulkanInstance& api, VkPhysicalDevice pdev);
    void                    waitIdleSync(Queue* q, size_t n);

    void                    pickPhysicalDevice();
    bool                    isDeviceSuitable(VkPhysicalDevice device, std::string_view gpuName);
    void                    deviceQueueProps(VkPhysicalDevice device, VkProps& prop);
    bool                    checkDeviceExtensionSupport(VkPhysicalDevice device);

    SwapChainSupport        querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    void                    createLogicalDevice(VulkanInstance& api, VkPhysicalDevice pdev);
  };

}}
