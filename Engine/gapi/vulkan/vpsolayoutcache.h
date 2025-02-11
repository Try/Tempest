#pragma once

#include <vector>
#include <mutex>

#include "gapi/shaderreflection.h"
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VPsoLayoutCache {
  public:
    VPsoLayoutCache(VDevice& dev);
    ~VPsoLayoutCache();

    VkPipelineLayout      findLayout(const ShaderReflection::PushBlock &pb, VkDescriptorSetLayout lay);
    VkPipelineLayout      findLayout(const ShaderReflection::PushBlock &pb, const ShaderReflection::LayoutDesc& lay);

  private:
    struct Layout {
      VkShaderStageFlags    pushStage = 0;
      uint32_t              pushSize  = 0;
      VkDescriptorSetLayout lay       = VK_NULL_HANDLE;
      VkPipelineLayout      pLay      = VK_NULL_HANDLE;
      };

    VDevice&             dev;
    std::mutex           sync;
    std::vector<Layout>  layouts;
  };

}
}
