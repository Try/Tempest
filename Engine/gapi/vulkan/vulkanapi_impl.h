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

    std::vector<VkExtensionProperties> instExtensionsList() const;

  private:
    static const std::initializer_list<const char*>& checkValidationLayerSupport();
    static bool layerSupport(const std::vector<VkLayerProperties>& sup,const std::initializer_list<const char*> dest);

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
