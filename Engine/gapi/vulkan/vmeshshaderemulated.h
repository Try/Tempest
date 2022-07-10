#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "gapi/shaderreflection.h"
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VMeshShaderEmulated:public AbstractGraphicsApi::Shader {
  public:
    VMeshShaderEmulated(VDevice& device, const void* source, size_t src_size);
    ~VMeshShaderEmulated();

    using Binding = ShaderReflection::Binding;

    VkShaderModule                   countPass = VK_NULL_HANDLE;
    VkShaderModule                   geomPass  = VK_NULL_HANDLE;

    std::vector<Decl::ComponentType> vdecl;
    std::vector<Binding>             lay;

    struct Comp {
      IVec3 wgSize;
      } comp;
  private:
    VkDevice                         device;
  };

}}
