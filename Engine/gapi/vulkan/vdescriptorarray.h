#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "vuniformslay.h"

namespace Tempest {
namespace Detail {

class VUniformsLay;

class VDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    VkDevice              device=nullptr;
    Detail::DSharedPtr<VUniformsLay*> lay;
    VkDescriptorSet       desc=VK_NULL_HANDLE;

    VDescriptorArray(VkDevice device, VUniformsLay& vlay);
    ~VDescriptorArray() override;

    void                     set    (size_t id, AbstractGraphicsApi::Texture* tex, const Sampler2d& smp) override;
    void                     setSsbo(size_t id, AbstractGraphicsApi::Texture* tex, uint32_t mipLevel) override;
    void                     setUbo (size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset, size_t size, size_t align) override;
    void                     setSsbo(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset, size_t size, size_t align) override;

  private:
    Detail::VUniformsLay::Pool* pool=nullptr;

    VkDescriptorPool         allocPool(const VUniformsLay& lay, size_t size);
    bool                     allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay);
    static void              addPoolSize(VkDescriptorPoolSize* p, size_t& sz, VkDescriptorType elt);
  };

}}
