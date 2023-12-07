#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "vshader.h"
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VTaskShaderEmulated : public VShader {
  public:
  VTaskShaderEmulated(VDevice& device, const void* source, size_t src_size);
  ~VTaskShaderEmulated();

  VkShaderModule compPass = VK_NULL_HANDLE;
  };

class VMeshShaderEmulated : public VShader {
  public:
    VMeshShaderEmulated(VDevice& device, const void* source, size_t src_size);
    ~VMeshShaderEmulated();

    VkShaderModule compPass = VK_NULL_HANDLE;
  };

}}
