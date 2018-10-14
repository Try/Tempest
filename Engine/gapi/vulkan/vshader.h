#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDevice;

class VShader:public AbstractGraphicsApi::Shader {
  public:
    VShader(VDevice& device,const char* file);
    ~VShader();

    VkShaderModule impl;

  private:
    VkDevice device;

    void cleanup();

    std::unique_ptr<uint32_t[]> load(const char* file, uint32_t &size);
  };

}}
