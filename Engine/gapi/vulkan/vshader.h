#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/UniformsLayout>

#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VShader:public AbstractGraphicsApi::Shader {
  public:
    VShader(VDevice& device, const void* source, size_t src_size);
    ~VShader();

    using Binding = UniformsLayout::Binding;

    VkShaderModule       impl;
    std::vector<Binding> lay;

  private:
    VkDevice device;
  };

}}
