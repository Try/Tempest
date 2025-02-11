#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>
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

    size_t sizeofBuffer(size_t layoutBind, size_t arraylen) const override;

    enum {
      HEAP_RES = 0,
      HEAP_SMP = 1,
      HEAP_MAX = 2,
      };

    struct Param {
      UINT64  heapOffset    = 0;
      UINT64  heapOffsetSmp = 0;

      D3D12_DESCRIPTOR_RANGE_TYPE rgnType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      };

    struct Heap {
      D3D12_DESCRIPTOR_HEAP_TYPE type    = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      UINT                       numDesc = 0;
      };

    struct RootPrm {
      size_t   heap = 0;
      UINT     heapOffset = 0;
      uint32_t binding = 0;
      };

    Heap                        heaps[HEAP_MAX] = {};
    std::vector<RootPrm>        roots;
    uint32_t                    pushConstantId     = uint32_t(-1);
    uint32_t                    pushBaseInstanceId = uint32_t(-1);

    std::vector<Param>          prm;
    std::vector<Binding>        lay;

    LayoutDesc                  layout;
    ShaderReflection::PushBlock pb;

    SyncDesc                    sync;

    ComPtr<ID3D12RootSignature> impl;
    DxDevice&                   dev;

  private:
    struct Parameter final {
      D3D12_DESCRIPTOR_RANGE    rgn;
      uint32_t                  id = 0;
      D3D12_SHADER_VISIBILITY   visibility = D3D12_SHADER_VISIBILITY_ALL;
      };

    bool                        runtimeSized = false;

    void init(const LayoutDesc& lay, const ShaderReflection::PushBlock& pb, bool has_baseVertex_baseInstance);
    void init(const std::vector<Binding>& lay, const ShaderReflection::PushBlock& pb, bool has_baseVertex_baseInstance);
    void add (const ShaderReflection::Binding& b, D3D12_DESCRIPTOR_RANGE_TYPE type, std::vector<Parameter>& root);

    void adjustSsboBindings();
    void setupLayout(ShaderReflection::PushBlock& pb, LayoutDesc &lx, SyncDesc& sync,
                     const std::vector<ShaderReflection::Binding> *sh[], size_t cnt);
  };

}
}

