#pragma once

#include "vbindlesscache.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VPipeline;
class VCompPipeline;

class VPushDescriptor {
  public:
    using Bindings   = VBindlessCache::Bindings;
    using WriteInfo  = VBindlessCache::WriteInfo;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = VPipelineLay::LayoutDesc;

    VPushDescriptor(VDevice& dev);
    ~VPushDescriptor();
    void reset();

    VkDescriptorSet push(const VPipeline&     pso, const Bindings& binding);
    VkDescriptorSet push(const VCompPipeline& pso, const Bindings& binding);
    VkDescriptorSet push(const LayoutDesc&    lay, const Bindings& binding);

    static void     write(VDevice &dev, VkWriteDescriptorSet &wx, WriteInfo &infoW, uint32_t dstBinding,
                          ShaderReflection::Class cls, AbstractGraphicsApi::NoCopy *data, uint32_t offset, const Sampler &smp);

  private:
    VDevice& dev;

    struct Pool {
      Pool(VDevice& dev);
      VkDescriptorPool impl = VK_NULL_HANDLE;
      };

    VkDescriptorSet allocSet(const VkDescriptorSetLayout dLayout);
    VkDescriptorSet allocSet(const LayoutDesc& layout);

    std::vector<Pool> pool;
  };

}
}
