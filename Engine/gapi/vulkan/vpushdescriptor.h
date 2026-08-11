#pragma once

#include "vbindlesscache.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VPipeline;
class VCompPipeline;
class VBuffer;

class VPushDescriptor {
  public:
    using Bindings   = Detail::Bindings;
    using WriteInfo  = VBindlessCache::WriteInfo;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;

    VPushDescriptor(VDevice& dev);
    ~VPushDescriptor();
    void reset();

    void            onNextCmdChunk();

    void            pushHeap(VkCommandBuffer cmd, uint32_t* indices, const PushBlock& pb, const LayoutDesc& lay, const Bindings& binding);
    VkDescriptorSet push(const PushBlock &pb, const LayoutDesc& lay, const Bindings& binding);

    static void     write(VDevice &dev, VkWriteDescriptorSet &wx, WriteInfo &infoW, uint32_t dstBinding,
                          ShaderReflection::Class cls, AbstractGraphicsApi::NoCopy *data, uint32_t offset, const ComponentMapping& mapping, const Sampler &smp);

    static void     write(VDevice &dev, void* resPtr, void* smpPtr,
                          ShaderReflection::Class cls, AbstractGraphicsApi::NoCopy *data, uint32_t offset, const ComponentMapping& mapping, const Sampler &smp);
  private:
    VDevice& dev;

    enum {
      RES_ALLOC_SZ = 256>MaxBindings ? 256 : MaxBindings,
      SMP_ALLOC_SZ = MaxBindings,
      };

    struct ResPool {
      ResPool(VDevice& dev, uint32_t size);

      std::shared_ptr<VBuffer>   heapMem;
      uint8_t*                   hostPtr = nullptr;
      uint32_t                   dPtr    = 0;
      uint32_t                   alloc   = 0;
      };

    struct DescPool {
      DescPool(VDevice& dev);
      VkDescriptorPool impl = VK_NULL_HANDLE;
      };

    VkDescriptorSet allocSet(const VkDescriptorSetLayout dLayout);
    VkDescriptorSet allocSet(const LayoutDesc& layout);
    uint32_t        allocHeap(VkCommandBuffer cmd, const uint32_t sz, const uint32_t step);

    void            bindHeap(VkCommandBuffer cmd, bool res, bool smp);

    std::vector<DescPool> descPool;
    std::vector<ResPool>  resPool;
    std::vector<std::shared_ptr<VBuffer>> smpPool;

    VBuffer* lastResHeap = nullptr;
    VBuffer* lastSmpHeap = nullptr;
  };

}
}
