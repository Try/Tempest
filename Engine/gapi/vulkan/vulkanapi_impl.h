#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

#include "vulkan_sdk.h"
#include "utility/spinlock.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VCommandBuffer;
class VSemaphore;
class VFence;

class VulkanInstance {
  public:
    VulkanInstance(bool enableValidationLayers=true);
    ~VulkanInstance();

    std::vector<AbstractGraphicsApi::Props> devices() const;

    VkInstance       instance = VK_NULL_HANDLE;
    bool             hasDeviceFeatures2 = false;

    struct VkProp:Tempest::AbstractGraphicsApi::Props {
      uint32_t graphicsFamily=uint32_t(-1);
      uint32_t presentFamily =uint32_t(-1);

      size_t   nonCoherentAtomSize=0;
      size_t   bufferImageGranularity=0;

      bool     hasMemRq2          = false;
      bool     hasDedicatedAlloc  = false;
      bool     hasSync2           = false;
      bool     hasDeviceAddress   = false;
      bool     hasDynRendering    = false;
      bool     hasDescIndexing    = false;
      bool     hasDevGroup        = false;
      bool     hasBarycentricsNV  = false;
      };

    static bool checkForExt(const std::vector<VkExtensionProperties>& list, const char* name);

    std::vector<VkExtensionProperties> instExtensionsList() const;
    static std::vector<VkExtensionProperties> extensionsList(VkPhysicalDevice dev);

    void deviceProps(VkPhysicalDevice physicalDevice, VkProp& c) const;
    void devicePropsShort(VkPhysicalDevice physicalDevice, VkProp& props) const;

  private:
    const std::initializer_list<const char*>& checkValidationLayerSupport();
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

    const bool                                      validation = false;
    VkDebugReportCallbackEXT                        callback   = VK_NULL_HANDLE;
    PFN_vkDestroyDebugReportCallbackEXT             vkDestroyDebugReportCallbackEXT = nullptr;
  };

}}
