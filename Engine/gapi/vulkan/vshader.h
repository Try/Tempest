#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDevice;

class VShader:public AbstractGraphicsApi::Shader {
  public:
    VShader(VDevice& device, const char* source, size_t src_size);
    ~VShader();

    VkShaderModule impl;

  private:
    VkDevice device;
  };

}}
