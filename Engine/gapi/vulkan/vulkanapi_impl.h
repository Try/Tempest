#pragma once

#include <vulkan/vulkan.hpp>
#include <Tempest/AbstractGraphicsApi>

#include "utility/spinlock.h"

namespace Tempest {
namespace Detail {

class VulkanApi {
  public:
    VulkanApi(bool enableValidationLayers=true);
    ~VulkanApi();

    std::vector<AbstractGraphicsApi::Props> devices() const;

    VkInstance       instance;

    struct VkProp:Tempest::AbstractGraphicsApi::Props {
      uint32_t graphicsFamily=uint32_t(-1);
      uint32_t presentFamily =uint32_t(-1);

      size_t   nonCoherentAtomSize=0;
      size_t   bufferImageGranularity=0;

      bool     hasMemRq2        =false;
      bool     hasDedicatedAlloc=false;
      };

    static void      getDeviceProps(VkPhysicalDevice physicalDevice, VkProp& c);
    static void      getDevicePropsShort(VkPhysicalDevice physicalDevice, AbstractGraphicsApi::Props& c);

  private:
    const std::initializer_list<const char*> checkValidationLayerSupport();
    bool layerSupport(const std::vector<VkLayerProperties>& sup,const std::initializer_list<const char*> dest);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
        VkDebugReportFlagsEXT       flags,
        VkDebugReportObjectTypeEXT  objectType,
        uint64_t                    object,
        size_t                      location,
        int32_t                     messageCode,
        const char*                 pLayerPrefix,
        const char*                 pMessage,
        void*                       pUserData);

    const bool                                      validation;
    VkDebugReportCallbackEXT                        callback;
    PFN_vkDestroyDebugReportCallbackEXT             vkDestroyDebugReportCallbackEXT = nullptr;
  };

}}
