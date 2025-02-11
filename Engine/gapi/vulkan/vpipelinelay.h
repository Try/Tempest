#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "vulkan_sdk.h"
#include <vector>

#include "gapi/shaderreflection.h"

namespace Tempest {

class PipelineLayout;

namespace Detail {

class VDevice;
class VShader;

class VPipelineLay : public AbstractGraphicsApi::PipelineLay {
  public:
    VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh);
    VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt);
    ~VPipelineLay();

    using Binding    = ShaderReflection::Binding;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    using SyncDesc   = ShaderReflection::SyncDesc;

    size_t                sizeofBuffer(size_t layoutBind, size_t arraylen) const override;

    PushBlock             pb;
    LayoutDesc            layout;
    SyncDesc              sync;

    VkDescriptorSetLayout dLay = VK_NULL_HANDLE;

  private:
    void                  setupLayout(PushBlock& pb, LayoutDesc &lx, SyncDesc& sync, const std::vector<Binding> *sh[], size_t cnt);
  };

}
}
