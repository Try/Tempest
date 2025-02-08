#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"
#include "utility/spinlock.h"

#include "dxfbolayout.h"
#include "dxpipelinelay.h"

namespace Tempest {
namespace Detail {

class DxDevice;
class DxShader;

class DxPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    DxPipeline() = delete;
    DxPipeline(DxDevice &device,
               const RenderState &st, Topology tp, const DxPipelineLay& ulay,
               const DxShader*const* shaders, size_t cnt);

    struct Inst final {
      Inst() = default;
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      DxFboLayout                 lay;
      ComPtr<ID3D12PipelineState> impl;
      };

    ComPtr<ID3D12RootSignature> sign;
    const DxPipelineLay&        layout;

    D3D_PRIMITIVE_TOPOLOGY      topology           = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    size_t                      pushConstantId     = 0;
    uint32_t                    pushBaseInstanceId = 0;

    ID3D12PipelineState&        instance(DXGI_FORMAT  frm);
    ID3D12PipelineState&        instance(const DxFboLayout& frm);

    IVec3                       workGroupSize() const override;

  private:
    DxDevice&                   device;
    DSharedPtr<const DxShader*> modules[5] = {};
    IVec3                       wgSize = {};

    UINT                        declSize=0;
    RenderState                 rState;
    std::unique_ptr<D3D12_INPUT_ELEMENT_DESC[]> vsInput;

    std::vector<Inst>           inst;
    SpinLock                    sync;

    const DxShader*             findShader(ShaderReflection::Stage sh) const;
    D3D12_BLEND_DESC            getBlend(const RenderState &st) const;
    D3D12_RASTERIZER_DESC       getRaster(const RenderState &st) const;
    D3D12_DEPTH_STENCIL_DESC    getDepth(const RenderState &st, DXGI_FORMAT depthFrm) const;
    D3D12_DEPTH_STENCIL_DESC1   getDepth1(const RenderState &st, DXGI_FORMAT depthFrm) const;
    ComPtr<ID3D12PipelineState> initGraphicsPipeline(const DxFboLayout& frm);
    ComPtr<ID3D12PipelineState> initMeshPipeline(const DxFboLayout& frm);
  };

class DxCompPipeline : public AbstractGraphicsApi::CompPipeline {
  public:
    DxCompPipeline() = delete;
    DxCompPipeline(DxDevice &device,
                   const DxPipelineLay& ulay,
                   DxShader& comp);

    IVec3                       workGroupSize() const;

    ComPtr<ID3D12RootSignature> sign;
    ComPtr<ID3D12PipelineState> impl;
    IVec3                       wgSize;
    const DxPipelineLay&        layout;
    size_t                      pushConstantId = 0;
  };

}}
