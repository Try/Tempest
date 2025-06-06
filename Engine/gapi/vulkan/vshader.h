#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "gapi/shader.h"
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VShader : public Tempest::Detail::Shader {
  public:
    VShader(VDevice& device, const void* source, size_t src_size);
    explicit VShader(VDevice& device);
    ~VShader();

    VkShaderModule impl = VK_NULL_HANDLE;

  protected:
    VkDevice       device;
  };

}}
