#pragma once

#include <vector>
#include <mutex>
#include "vulkan_sdk.h"

#include "gapi/abstractgraphicsapi.h"
#include "gapi/shaderreflection.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VPipeline;
class VCompPipeline;

class VBindlessCache {
  public:
    VBindlessCache(VDevice& dev);
    ~VBindlessCache();

    union WriteInfo {
      VkDescriptorImageInfo                        image;
      VkDescriptorBufferInfo                       buffer;
      VkWriteDescriptorSetAccelerationStructureKHR tlas;
      };

    struct Inst {
      VkDescriptorSet       set  = VK_NULL_HANDLE;
      VkDescriptorSetLayout dLay = VK_NULL_HANDLE;
      VkPipelineLayout      pLay = VK_NULL_HANDLE;
      };

    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;

    Inst inst(const PushBlock &pb, const LayoutDesc& layout, const Bindings& binding);

    void notifyDestroy(const AbstractGraphicsApi::NoCopy* res);

  private:
    struct DSet {
      VkDescriptorSetLayout dLay = VK_NULL_HANDLE;
      Bindings              bindings;

      VkDescriptorPool      pool = VK_NULL_HANDLE;
      VkDescriptorSet       set  = VK_NULL_HANDLE;
      };

    void             addPoolSize(VkDescriptorPoolSize *p, size_t &sz, uint32_t cnt, VkDescriptorType elt);
    VkDescriptorPool allocPool(const LayoutDesc &l);
    VkDescriptorSet  allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay);

    void             initDescriptorSet(VkDescriptorSet set, const Bindings& binding, const LayoutDesc &lx);

    VDevice&             dev;
    std::mutex           syncDesc;
    std::vector<DSet>    descriptors;
  };

}
}
