#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "vulkan_sdk.h"
#include <mutex>
#include <list>
#include <vector>

#include "gapi/shaderreflection.h"
#include "utility/spinlock.h"

namespace Tempest {

class PipelineLayout;

namespace Detail {

class VDescriptorArray;
class VDevice;
class VShader;

class VPipelineLay : public AbstractGraphicsApi::PipelineLay {
  public:
    VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt);
    ~VPipelineLay();

    size_t descriptorsCount() override;

    using Binding = ShaderReflection::Binding;

    VkDevice                      dev =nullptr;
    VkDescriptorSetLayout         impl=VK_NULL_HANDLE;
    std::vector<Binding>          lay;
    ShaderReflection::PushBlock   pb;
    bool                          hasSSBO = false;

  private:
    enum {
      POOL_SIZE=512
      };

    struct Pool {
      VkDescriptorPool impl      = VK_NULL_HANDLE;
      uint16_t         freeCount = POOL_SIZE;
      };

    Detail::SpinLock sync;
    std::list<Pool>  pool;

    void implCreate(VkDescriptorSetLayoutBinding *bind);
    void adjustSsboBindings();

  friend class VDescriptorArray;
  };

}
}
