#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "vuniformslay.h"

namespace Tempest {
namespace Detail {

class VUniformsLay;

class VDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    VDescriptorArray(VkDevice device, VUniformsLay& vlay);
    ~VDescriptorArray() override;

    void                     set    (size_t id, AbstractGraphicsApi::Texture* tex, const Sampler2d& smp) override;
    void                     setSsbo(size_t id, AbstractGraphicsApi::Texture* tex, uint32_t mipLevel) override;
    void                     setUbo (size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset, size_t size, size_t align) override;
    void                     setSsbo(size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset, size_t size, size_t align) override;
    void                     ssboBarriers(Detail::ResourceState& res) override;

    VkDescriptorSet           desc=VK_NULL_HANDLE;

  private:
    VkDevice                  device=nullptr;
    DSharedPtr<VUniformsLay*> lay;
    VUniformsLay::Pool*       pool=nullptr;

    struct SSBO {
      AbstractGraphicsApi::Texture* tex = nullptr;
      AbstractGraphicsApi::Buffer*  buf = nullptr;
      };
    std::unique_ptr<SSBO[]>  ssbo;

    VkDescriptorPool         allocPool(const VUniformsLay& lay, size_t size);
    bool                     allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay);
    static void              addPoolSize(VkDescriptorPoolSize* p, size_t& sz, VkDescriptorType elt);
  };

}}
