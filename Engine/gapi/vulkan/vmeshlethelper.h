#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "vbuffer.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VPipelineLay;
class VCompPipeline;

class VMeshletHelper {
  public:
    enum {
      PipelineMemorySize = 16*1024*1024,
      MeshletsMemorySize = 16*1024,
      IndirectMemorySize = 32*1024,
      };
    explicit VMeshletHelper(VDevice& dev);
    ~VMeshletHelper();

    void bindCS(VkCommandBuffer impl, VkPipelineLayout lay);
    void bindVS(VkCommandBuffer impl);

    void initRP(VkCommandBuffer impl);

    void sortPass(VkCommandBuffer impl, uint32_t meshCallsCount);

    VkDescriptorSetLayout lay() const { return descLay; }

  private:
    void                  cleanup();
    VkDescriptorSetLayout initLayout(VDevice& device);
    VkDescriptorPool      initPool(VDevice& device);
    VkDescriptorSet       initDescriptors(VDevice& device, VkDescriptorPool pool, VkDescriptorSetLayout lay);
    void                  initShaders(VDevice& device);
    void                  initEngSet();

    VDevice&              dev;
    VBuffer               indirect, meshlets, scratch, compacted;

    VkDescriptorSetLayout descLay  = VK_NULL_HANDLE;
    VkDescriptorPool      descPool = VK_NULL_HANDLE;
    VkDescriptorSet       engSet   = VK_NULL_HANDLE;

    DSharedPtr<VPipelineLay*>  prefixSumLay;
    DSharedPtr<VCompPipeline*> prefixSum;
    VkDescriptorSet            sumSet   = VK_NULL_HANDLE;
  };

}
}
