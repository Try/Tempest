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
  private:
    struct IndirectCommand {
      uint32_t indexCount;
      };

    struct DrawIndexedIndirectCommand {
      uint32_t drawId;
      uint32_t indexCountSrc;
      uint32_t indexCount;
      uint32_t instanceCount;
      uint32_t firstIndex;    // prefix sum
      int32_t  vertexOffset;  // can be abused to offset into var_buffer
      uint32_t firstInstance; // caps: should be zero
      uint32_t vboOffset;     // prefix sum
      };

  public:
    enum {
      MeshletsMaxCount   = 1024*32,
      IndirectCmdCount   = 8096,
      PipelinewordsCount = 32*1024*1024,
      PipelineMemorySize = PipelinewordsCount*4,
      MeshletsMemorySize = MeshletsMaxCount*3*4,

      IndirectScratchSize = IndirectCmdCount*sizeof(IndirectCommand),
      IndirectMemorySize  = IndirectCmdCount*sizeof(VkDrawIndexedIndirectCommand),
      };
    explicit VMeshletHelper(VDevice& dev);
    ~VMeshletHelper();

    void bindCS(VkPipelineLayout task, VkPipelineLayout mesh);
    void bindVS(VkCommandBuffer impl, VkPipelineLayout lay);

    void initRP(VkCommandBuffer impl);

    void drawCompute(VkCommandBuffer task, VkCommandBuffer mesh, uint32_t taskId, uint32_t drawId, size_t x, size_t y, size_t z);
    void drawIndirect(VkCommandBuffer impl, uint32_t drawId);
    void taskEpiloguePass(VkCommandBuffer impl, uint32_t meshCallsCount);
    void sortPass(VkCommandBuffer impl, uint32_t meshCallsCount);

    VkDescriptorSetLayout lay() const { return engLay; }

  private:
    void                  cleanup();
    VkDescriptorSetLayout initLayout(VDevice& device);
    VkDescriptorSetLayout initDrawLayout(VDevice& device);
    VkDescriptorPool      initPool(VDevice& device, uint32_t cnt);
    VkDescriptorSet       initDescriptors(VDevice& device, VkDescriptorPool pool, VkDescriptorSetLayout lay);
    void                  initShaders(VDevice& device);

    void                  initEngSet (VkDescriptorSet set, uint32_t cnt, bool dynamic);
    void                  initDrawSet(VkDescriptorSet set);

    void                  barrier(VkCommandBuffer impl,
                                  VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                  VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);

    VDevice&                   dev;
    VBuffer                    indirect, meshlets, scratch;

    VkDescriptorSetLayout      engLay   = VK_NULL_HANDLE;
    VkDescriptorPool           engPool  = VK_NULL_HANDLE;
    VkDescriptorSet            engSet   = VK_NULL_HANDLE;

    VkDescriptorSet            compPool = VK_NULL_HANDLE;
    VkDescriptorSet            compSet  = VK_NULL_HANDLE;

    VkDescriptorSetLayout      drawLay  = VK_NULL_HANDLE;
    VkDescriptorSet            drawPool = VK_NULL_HANDLE;
    VkDescriptorSet            drawSet  = VK_NULL_HANDLE;

    VkPipelineLayout           currentTaskLayout = VK_NULL_HANDLE;
    VkPipelineLayout           currentMeshLayout = VK_NULL_HANDLE;

    uint32_t                   indirectRate    = 1;
    uint32_t                   indirectOffset  = 0;

    DSharedPtr<VPipelineLay*>  initLay;
    DSharedPtr<VCompPipeline*> init;

    DSharedPtr<VPipelineLay*>  taskPostPassLay;
    DSharedPtr<VCompPipeline*> taskPostPass;

    DSharedPtr<VPipelineLay*>  taskLutPassLay;
    DSharedPtr<VCompPipeline*> taskLutPass;

    DSharedPtr<VPipelineLay*>  prefixSumLay;
    DSharedPtr<VCompPipeline*> prefixSum;

    DSharedPtr<VPipelineLay*>  compactageLay;
    DSharedPtr<VCompPipeline*> compactage;
  };

}
}
