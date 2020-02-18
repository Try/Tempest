#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {
namespace Detail {

class DxDevice;
class DxShader;
class DxUniformsLay;

class DxPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    DxPipeline(DxDevice &device,
               const RenderState &st,
               const Decl::ComponentType *decl, size_t declSize,
               size_t stride, Topology tp,
               const DxUniformsLay& ulay,
               DxShader &vert, DxShader &frag);

  private:
    //ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;

    D3D12_BLEND_DESC            getBlend(const RenderState &st) const;
    D3D12_RASTERIZER_DESC       getRaster(const RenderState &st) const;
  };

}}
