#pragma once

#include <vector>
#include <mutex>

#include "gapi/shaderreflection.h"
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VSetLayoutCache {
  public:
    VSetLayoutCache(VDevice& dev);
    ~VSetLayoutCache();

    VkDescriptorSetLayout findLayout(const ShaderReflection::LayoutDesc &l);

  private:
    using LayoutDesc = ShaderReflection::LayoutDesc;

    struct Layout {
      LayoutDesc            desc;
      VkDescriptorSetLayout lay;
      };

    VDevice&             dev;
    std::mutex           syncLay;
    std::vector<Layout>  layouts;
  };

}
}
