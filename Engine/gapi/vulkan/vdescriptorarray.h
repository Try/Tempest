#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VUniformsLay;

class VDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    VkDevice              device=nullptr;
    VkDescriptorPool      impl =VK_NULL_HANDLE;
    std::shared_ptr<AbstractGraphicsApi::UniformsLay> lay;
    VkDescriptorSet       desc[1]={};

    VDescriptorArray(VkDevice device, const UniformsLayout& lay, std::shared_ptr<AbstractGraphicsApi::UniformsLay> &layImpl);
    ~VDescriptorArray() override;

    void                     set   (size_t id, AbstractGraphicsApi::Texture *tex) override;
    void                     set   (size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset, size_t size) override;

    static void              addPoolSize(VkDescriptorPoolSize* p, size_t& sz, VkDescriptorType elt);
  };

}}
