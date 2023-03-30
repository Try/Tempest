#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "utility/smallarray.h"
#include "gapi/resourcestate.h"
#include "vpipelinelay.h"

#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VPipelineLay;

class VDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    VDescriptorArray(VDevice& device, VPipelineLay& vlay);
    ~VDescriptorArray() override;

    enum {
      ALLOC_GRANULARITY = 4,
      };

    void                      set    (size_t id, AbstractGraphicsApi::Texture* tex, const Sampler& smp, uint32_t mipLevel) override;
    void                      set    (size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset) override;
    void                      set    (size_t id, const Sampler& smp) override;
    void                      setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) override;

    void                      set    (size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt, const Sampler& smp, uint32_t mipLevel) override;
    void                      set    (size_t id, AbstractGraphicsApi::Buffer**  buf, size_t cnt) override;

    void                      ssboBarriers(Detail::ResourceState& res, PipelineStage st) override;

    bool                      isRuntimeSized() const;
    VkPipelineLayout          pipelineLayout() { return dedicatedLayout; }

    VkDescriptorSet           impl             = VK_NULL_HANDLE;

  private:
    VDevice&                  device;
    DSharedPtr<VPipelineLay*> lay;
    VPipelineLay::Pool*       pool = nullptr;

    VkPipelineLayout          dedicatedLayout = VK_NULL_HANDLE;
    VkDescriptorPool          dedicatedPool   = VK_NULL_HANDLE;
    std::vector<uint32_t>     runtimeArrays;

    struct UAV {
      AbstractGraphicsApi::Texture* tex = nullptr;
      AbstractGraphicsApi::Buffer*  buf = nullptr;
      };
    SmallArray<UAV,16>        uav;
    ResourceState::Usage      uavUsage;

    VkDescriptorPool          allocPool(const VPipelineLay& lay);
    VkDescriptorSet           allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay);
    static void               addPoolSize(VkDescriptorPoolSize* p, size_t& sz, uint32_t cnt, VkDescriptorType elt);
    void                      reallocSet(size_t id, uint32_t oldRuntimeSz);
  };

}}
