#pragma once

#include "vbindlesscache.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VPipeline;
class VCompPipeline;

class VPushDescriptor {
  public:
    using Bindings   = Detail::Bindings;
    using WriteInfo  = VBindlessCache::WriteInfo;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;

    VPushDescriptor(VDevice& dev);
    ~VPushDescriptor();
    void reset();

    VkDescriptorSet push(const PushBlock &pb, const LayoutDesc& lay, const Bindings& binding);

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
