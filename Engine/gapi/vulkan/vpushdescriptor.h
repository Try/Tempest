#pragma once

#include "vulkan_sdk.h"
#include "gapi/shaderreflection.h"
#include <cstdint>

namespace Tempest {
namespace Detail {

class VDevice;
class VPipeline;
class VCompPipeline;
class VBuffer;
class VDescriptorHeap;

class VPushDescriptor {
  public:
    using Bindings   = Detail::Bindings;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    union WriteInfo {
      VkDescriptorImageInfo                        image;
      VkDescriptorBufferInfo                       buffer;
      VkWriteDescriptorSetAccelerationStructureKHR tlas;
      };

    VPushDescriptor(VDevice& dev);
    ~VPushDescriptor();
    void reset();

    void            onNextCmdChunk();

    void            pushHeap(VkCommandBuffer cmd, uint32_t* indices, const PushBlock& pb, const LayoutDesc& lay, const Bindings& binding);
    VkDescriptorSet push(const PushBlock &pb, const LayoutDesc& lay, const Bindings& binding);

    static void     write(VDevice &dev, VkWriteDescriptorSet &wx, WriteInfo &infoW, uint32_t dstBinding,
                          ShaderReflection::Class cls, AbstractGraphicsApi::NoCopy *data, uint32_t offset, const ComponentMapping& mapping, const Sampler &smp);

    static void     write(VDevice &dev, void* resPtr,
                          ShaderReflection::Class cls, AbstractGraphicsApi::NoCopy *data, uint32_t offset, const ComponentMapping& mapping);
  private:
    VDevice& dev;

    enum {
      RES_ALLOC_SZ = 256>MaxBindings ? 256 : MaxBindings,
      SMP_ALLOC_SZ = MaxBindings,
      };

    struct ResPool {
      ResPool(VDevice& dev, uint32_t size);

      uint32_t dPtr  = 0;
      uint32_t alloc = 0;
      };

    struct DescPool {
      DescPool(VDevice& dev);
      VkDescriptorPool impl = VK_NULL_HANDLE;
      };

    VkDescriptorSet allocSet(const VkDescriptorSetLayout dLayout);
    VkDescriptorSet allocSet(const LayoutDesc& layout);
    uint32_t        allocHeap(VkCommandBuffer cmd, const uint32_t sz, const uint32_t step);

    void            bindHeap(VkCommandBuffer cmd, const DSharedPtr<VDescriptorHeap*>& res, const DSharedPtr<VDescriptorHeap*>& smp);

    std::vector<DescPool> descPool;
    std::vector<ResPool>  resPool;
    std::vector<DSharedPtr<VDescriptorHeap*>> memHeap;

    VDescriptorHeap* lastResHeap = nullptr;
    VDescriptorHeap* lastSmpHeap = nullptr;
  };

}
}
