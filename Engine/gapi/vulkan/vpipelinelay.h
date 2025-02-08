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
    VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh);
    VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt);
    ~VPipelineLay();

    using Binding    = ShaderReflection::Binding;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    using SyncDesc   = ShaderReflection::SyncDesc;

    struct DedicatedLay {
      VkDescriptorSetLayout dLay = VK_NULL_HANDLE;
      VkPipelineLayout      pLay = VK_NULL_HANDLE;
      };

    size_t                descriptorsCount() override;
    size_t                sizeofBuffer(size_t layoutBind, size_t arraylen) const override;

    DedicatedLay          create(const std::vector<uint32_t>& runtimeArrays);
    bool                  isUpdateAfterBind() const;

    VDevice&                    dev;
    LayoutDesc                  layout;
    SyncDesc                    sync;

    VkDescriptorSetLayout       impl     = VK_NULL_HANDLE;
    VkDescriptorSetLayout       msHelper = VK_NULL_HANDLE;

    std::vector<Binding>        lay;
    ShaderReflection::PushBlock pb;
    bool                        runtimeSized = false;

  private:
    enum {
      POOL_SIZE    = 512,
      // MAX_BINDLESS = 4096,
      };

    struct Pool {
      VkDescriptorPool impl      = VK_NULL_HANDLE;
      uint16_t         freeCount = POOL_SIZE;
      };

    struct DLay : DedicatedLay {
      std::vector<uint32_t> runtimeArrays;
      };

    Detail::SpinLock syncPool;
    std::list<Pool>  pool;

    Detail::SpinLock  syncLay;
    std::vector<DLay> dedicatedLay;

    VkDescriptorSetLayout createDescLayout(const std::vector<uint32_t>& runtimeArrays) const;
    VkDescriptorSetLayout createMsHelper() const;

    void                  adjustSsboBindings();
    void                  setupLayout(LayoutDesc &lx, SyncDesc& sync, const std::vector<ShaderReflection::Binding> *sh[], size_t cnt);

  friend class VDescriptorArray;
  };

}
}
