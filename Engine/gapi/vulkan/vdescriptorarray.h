#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

#include "vuniformslay.h"

namespace Tempest {
namespace Detail {

class VUniformsLay;

class VDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    VkDevice              device=nullptr;
    std::shared_ptr<AbstractGraphicsApi::UniformsLay> lay;
    VkDescriptorSet       desc=VK_NULL_HANDLE;

    VDescriptorArray(VkDevice device, const UniformsLayout& lay, std::shared_ptr<AbstractGraphicsApi::UniformsLay> &layImpl);
    ~VDescriptorArray() override;

    void                     set   (size_t id, AbstractGraphicsApi::Texture *tex) override;
    void                     set   (size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset, size_t size) override;

  private:
    Detail::VUniformsLay::Pool* pool=nullptr;

    VkDescriptorPool         allocPool(const UniformsLayout& lay,
                                       std::shared_ptr<AbstractGraphicsApi::UniformsLay>& layP, size_t size);
    bool                     allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay);
    static void              addPoolSize(VkDescriptorPoolSize* p, size_t& sz, VkDescriptorType elt);
  };

}}
