#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDescriptorArray : public AbstractGraphicsApi::Desc {
  private:
    VDescriptorArray()=default;
    ~VDescriptorArray();

  public:
    VkDevice         device=nullptr;
    VkDescriptorPool impl =VK_NULL_HANDLE;
    size_t           count=0;
    VkDescriptorSet  desc[1]={};

    static VDescriptorArray* alloc(VkDevice device, VkDescriptorSetLayout lay, size_t size);
    static void              free(VDescriptorArray* a);

    void                     bind(AbstractGraphicsApi::Buffer* memory, size_t size);
  };

}}
