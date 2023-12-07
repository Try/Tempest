#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "gapi/shaderreflection.h"
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VShader:public AbstractGraphicsApi::Shader {
  public:
    VShader(VDevice& device, const void* source, size_t src_size);
    explicit VShader(VDevice& device);
    ~VShader();

    using Binding = ShaderReflection::Binding;

    VkShaderModule                   impl = VK_NULL_HANDLE;
    std::vector<Decl::ComponentType> vdecl;
    std::vector<Binding>             lay;
    ShaderReflection::Stage          stage = ShaderReflection::Stage::Compute;

    struct Comp {
      IVec3 wgSize;
      } comp;

  protected:
    void                             fetchBindings(const uint32_t* source, size_t size);
    VkDevice                         device;
  };

}}
