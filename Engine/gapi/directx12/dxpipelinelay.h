#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"
#include "gapi/shaderreflection.h"

namespace Tempest {

class PipelineLayout;

namespace Detail {

class DxDescriptorArray;
class DxDevice;

class DxPipelineLay : public AbstractGraphicsApi::PipelineLay {
  public:
    DxPipelineLay(DxDevice& device, const std::vector<ShaderReflection::Binding>* sh);
    DxPipelineLay(DxDevice& device, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt,
                  bool has_baseVertex_baseInstance);

    using Binding    = ShaderReflection::Binding;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    using SyncDesc   = ShaderReflection::SyncDesc;

    uint32_t                    pushConstantId     = uint32_t(-1);
    uint32_t                    pushBaseInstanceId = uint32_t(-1);

    LayoutDesc                  layout;
    ShaderReflection::PushBlock pb;
    SyncDesc                    sync;

    ComPtr<ID3D12RootSignature> impl;
  };

}
}

