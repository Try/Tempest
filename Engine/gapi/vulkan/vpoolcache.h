#pragma once

#include <vector>
#include <mutex>

#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VulkanInstance;

class VPoolCache {
  public:
    VPoolCache(VDevice& dev);
    ~VPoolCache();

    void setupLimits();

    VkDescriptorPool allocPool();
    void             freePool(VkDescriptorPool p);

  private:
    static constexpr const size_t MaxCache = 2;

    VDevice&                      dev;
    VkPhysicalDeviceLimits        limits = {};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR rtLimits = {};

    std::mutex                    sync;
    std::vector<VkDescriptorPool> cache;
  };

}
}
