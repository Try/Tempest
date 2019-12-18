#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {

class UniformsLayout;

namespace Detail {

struct VUniformsLay : AbstractGraphicsApi::UniformsLay {
  VUniformsLay(VkDevice dev, const UniformsLayout& lay);
  ~VUniformsLay();

  VkDevice              dev =nullptr;
  VkDescriptorSetLayout impl=VK_NULL_HANDLE;
  std::vector<VkDescriptorType> hint;

  void implCreate(const UniformsLayout& lay, VkDescriptorSetLayoutBinding *bind);
  };

}
}
