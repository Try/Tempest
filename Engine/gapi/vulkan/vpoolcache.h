#pragma once

#include <vector>
#include <mutex>

#include "vulkan_sdk.h"

#include "gapi/shaderreflection.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VulkanInstance;

class VPoolCache {
  public:
    VPoolCache(VDevice& dev);
    ~VPoolCache();

    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    struct Inst {
      VkDescriptorSet       set  = VK_NULL_HANDLE;
      VkDescriptorSetLayout dLay = VK_NULL_HANDLE;
      VkPipelineLayout      pLay = VK_NULL_HANDLE;
      };

    void             setupLimits();
    void             notifyDestroy(const AbstractGraphicsApi::NoCopy* res);

    VkDescriptorPool allocPool();
    void             freePool(VkDescriptorPool p);

    Inst             allocBindless(const PushBlock &pb, const LayoutDesc& layout, const Bindings& binding);

  private:
    static constexpr const size_t MaxCache = 2;

    struct DSet {
      VkDescriptorSetLayout dLay = VK_NULL_HANDLE;
      Bindings              bindings;

      VkDescriptorPool      pool = VK_NULL_HANDLE;
      VkDescriptorSet       set  = VK_NULL_HANDLE;
      };

    VkDescriptorPool allocPool(const LayoutDesc &l);
    VkDescriptorSet  allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay);
    void             initDescriptorSet(VkDescriptorSet dset, const Bindings &binding, const LayoutDesc& l);

    VDevice&                      dev;
    VkPhysicalDeviceLimits        limits = {};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR rtLimits = {};

    std::mutex                    sync;
    std::vector<VkDescriptorPool> cache;
    std::vector<DSet>             descriptors;
  };

}
}
