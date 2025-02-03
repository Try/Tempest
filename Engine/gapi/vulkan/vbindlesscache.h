#pragma once

#include <vector>
#include <mutex>
#include "vulkan_sdk.h"

#include "gapi/abstractgraphicsapi.h"
#include "gapi/vulkan/vpipelinelay.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VPipelineLay;
class VPipeline;
class VCompPipeline;

class VBindlessCache {
  public:
    VBindlessCache(VDevice& dev);
    ~VBindlessCache();

    struct Bindings {
      AbstractGraphicsApi::NoCopy* data  [MaxBindings] = {};
      Sampler                      smp   [MaxBindings] = {};
      uint32_t                     offset[MaxBindings] = {}; // ssbo offset or texture mip-level
      uint32_t                     array               = {};

      bool operator == (const Bindings& other) const;
      bool operator != (const Bindings& other) const;

      bool contains(const AbstractGraphicsApi::NoCopy* res) const;
      };

    union WriteInfo {
      VkDescriptorImageInfo                        image;
      VkDescriptorBufferInfo                       buffer;
      VkWriteDescriptorSetAccelerationStructureKHR tlas;
      };

    struct Inst {
      VkDescriptorSet       set  = VK_NULL_HANDLE;
      VkDescriptorSetLayout lay  = VK_NULL_HANDLE;
      VkPipelineLayout      pLay = VK_NULL_HANDLE;
      };

    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = VPipelineLay::LayoutDesc;

    Inst inst(const VPipeline&     pso, const Bindings& binding);
    Inst inst(const VCompPipeline& pso, const Bindings& binding);
    Inst inst(const PushBlock &pb, const LayoutDesc& layout, const Bindings& binding);

    void notifyDestroy(const AbstractGraphicsApi::NoCopy* res);

  private:
    struct PLayout {
      VkShaderStageFlags    pushStage = 0;
      uint32_t              pushSize  = 0;
      VkDescriptorSetLayout lay       = VK_NULL_HANDLE;
      VkPipelineLayout      pLay      = VK_NULL_HANDLE;
      };

    struct DSet {
      Bindings         bindings;

      VkDescriptorPool pool = VK_NULL_HANDLE;
      VkDescriptorSet  set  = VK_NULL_HANDLE;
      };

    PLayout          findPsoLayout(const ShaderReflection::PushBlock &pb, VkDescriptorSetLayout lay);

    void             addPoolSize(VkDescriptorPoolSize *p, size_t &sz, uint32_t cnt, VkDescriptorType elt);
    VkDescriptorPool allocPool(const LayoutDesc &l);
    VkDescriptorSet  allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay);

    void             initDescriptorSet(VkDescriptorSet set, const Bindings& binding, const LayoutDesc &lx);

    VDevice&             dev;

    std::mutex           syncPLay;
    std::vector<PLayout> pLayouts;

    std::mutex           syncDesc;
    std::vector<DSet>    descriptors;
  };

}
}
