#pragma once

#include <vector>
#include <mutex>
#include "vulkan_sdk.h"

#include "gapi/vulkan/vpipelinelay.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VSetLayoutCache {
  public:
    VSetLayoutCache(VDevice& dev);
    ~VSetLayoutCache();

    using LayoutDesc = VPipelineLay::LayoutDesc;

    struct Layout {
      LayoutDesc            desc;
      VkDescriptorSetLayout lay;
      };

    VSetLayoutCache::Layout findLayout(const LayoutDesc &l);

  private:
    VDevice&             dev;
    std::mutex           syncLay;
    std::vector<Layout>  layouts;
  };

}
}
