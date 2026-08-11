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

    void            pushHeap(uint32_t* indices, const PushBlock& pb, const LayoutDesc& lay, const Bindings& binding);
    void            bindHeap(VkCommandBuffer cmd);
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

    template<HeapType T>
    struct Pool {
      Pool(VDevice& dev, uint32_t size);

      std::shared_ptr<VBuffer>   heapMem;
      uint8_t*                   hostPtr = nullptr;
      uint32_t                   dPtr    = 0;
      uint32_t                   alloc   = 0;
      };

    template<>
    struct Pool<DESCRIPTOR_POOL> {
      Pool(VDevice& dev);
      VkDescriptorPool impl = VK_NULL_HANDLE;
      };

    using ResPool  = Pool<HEAP_TYPE_CBV_SRV_UAV>;
    using DescPool = Pool<DESCRIPTOR_POOL>;

    VkDescriptorSet allocSet(const VkDescriptorSetLayout dLayout);
    VkDescriptorSet allocSet(const LayoutDesc& layout);

    template<HeapType T>
    uint32_t                      allocHeap(std::vector<Pool<T>>& pool, const uint32_t sz, const uint32_t step);

    std::vector<DescPool> descPool;
    std::vector<ResPool>  resPool;
  };

}
}
