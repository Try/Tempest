#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDescriptorArray : public AbstractGraphicsApi::Desc {
  private:
    VDescriptorArray()=default;
    ~VDescriptorArray() override;

  public:
    VkDevice         device=nullptr;
    VkDescriptorPool impl =VK_NULL_HANDLE;
    size_t           count=0;
    VkDescriptorSet  desc[1]={};

    static VDescriptorArray* alloc(VkDevice device, VkDescriptorSetLayout lay);
    static void              free(VDescriptorArray* a);

    void                     set(size_t id, AbstractGraphicsApi::Texture *tex) override;
    void                     set(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset, size_t size) override;
  };

}}
