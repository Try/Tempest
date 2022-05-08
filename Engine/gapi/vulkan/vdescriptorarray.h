#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "vpipelinelay.h"

namespace Tempest {
namespace Detail {

class VPipelineLay;

class VDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    VDescriptorArray(VkDevice device, VPipelineLay& vlay);
    ~VDescriptorArray() override;

    void                     set    (size_t id, AbstractGraphicsApi::Texture* tex, const Sampler2d& smp) override;
    void                     setSsbo(size_t id, AbstractGraphicsApi::Texture* tex, uint32_t mipLevel) override;
    void                     setUbo (size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset) override;
    void                     setSsbo(size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset) override;
    void                     setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) override;

    void                     set    (size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt, const Sampler2d& smp) override;

    void                     ssboBarriers(Detail::ResourceState& res) override;

    VkDescriptorSet           desc=VK_NULL_HANDLE;

  private:
    VkDevice                  device=nullptr;
    DSharedPtr<VPipelineLay*> lay;
    VPipelineLay::Pool*       pool=nullptr;

    VkDescriptorPool          dedicatedPool   = VK_NULL_HANDLE;
    VkDescriptorSetLayout     dedicatedLayout = VK_NULL_HANDLE;
    uint32_t                  runtimeArraySz  = 0; // TODO: per bind

    struct SSBO {
      AbstractGraphicsApi::Texture* tex = nullptr;
      AbstractGraphicsApi::Buffer*  buf = nullptr;
      };
    std::unique_ptr<SSBO[]>  ssbo;

    VkDescriptorPool         allocPool(const VPipelineLay& lay, size_t size);
    VkDescriptorSet          allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay);
    static void              addPoolSize(VkDescriptorPoolSize* p, size_t& sz, uint32_t cnt, VkDescriptorType elt);
    void                     reallocSet();
  };

}}
