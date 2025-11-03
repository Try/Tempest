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
    DxPipeline(DxDevice &device, const RenderState &st, Topology tp, const DxShader*const* shaders, size_t cnt);

    using Binding    = ShaderReflection::Binding;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    using SyncDesc   = ShaderReflection::SyncDesc;

    IVec3                       workGroupSize() const override;
    size_t                      sizeofBuffer(size_t id, size_t arraylen) const override;

    ID3D12PipelineState&        instance(DXGI_FORMAT  frm);
    ID3D12PipelineState&        instance(const DxFboLayout& frm);

    PushBlock                   pb;
    LayoutDesc                  layout;
    SyncDesc                    sync;

    ComPtr<ID3D12RootSignature> sign;
    D3D_PRIMITIVE_TOPOLOGY      topology           = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    uint32_t                    pushConstantId     = 0;
    uint32_t                    pushBaseInstanceId = 0;

  private:
    struct Inst final {
      Inst() = default;
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      DxFboLayout                 lay;
      ComPtr<ID3D12PipelineState> impl;
      };

    DxDevice&                   device;
    DSharedPtr<const DxShader*> modules[5] = {};
    IVec3                       wgSize = {};

    UINT                        declSize=0;
    RenderState                 rState;
    std::unique_ptr<D3D12_INPUT_ELEMENT_DESC[]> vsInput;

    SpinLock                    syncInst;
    std::vector<Inst>           inst;

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
    DxCompPipeline(DxDevice &device, DxShader& comp);

    using Binding    = ShaderReflection::Binding;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    using SyncDesc   = ShaderReflection::SyncDesc;

    IVec3                       workGroupSize() const override;
    size_t                      sizeofBuffer(size_t id, size_t arraylen) const override;

    PushBlock                   pb;
    LayoutDesc                  layout;
    SyncDesc                    sync;

    ComPtr<ID3D12RootSignature> sign;
    ComPtr<ID3D12PipelineState> impl;
    IVec3                       wgSize;
    uint32_t                    pushConstantId = 0;
  };

}}
