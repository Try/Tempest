#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxpipeline.h"

#include "dxdevice.h"
#include "dxshader.h"
#include "dxuniformslay.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxPipeline::DxPipeline(DxDevice& device, const RenderState& st,
                       const Decl::ComponentType* decl, size_t declSize, size_t stride,
                       Topology tp, const DxUniformsLay& ulay,
                       DxShader& vert, DxShader& frag)
  :stride(UINT(stride)){
  static const DXGI_FORMAT vertFormats[]={
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,

    DXGI_FORMAT_R8G8B8A8_UNORM, //TODO: test byteorder

    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16B16A16_SNORM,

    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    };

  static const uint32_t vertSize[]={
    0,
    4,
    8,
    12,
    16,

    4,

    4,
    8,

    4,
    8
  };

  static const D3D_PRIMITIVE_TOPOLOGY dxTopolgy[]={
    D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,
    D3D_PRIMITIVE_TOPOLOGY_LINELIST,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
  };

  topology = dxTopolgy[int(tp)];

  D3D12_INPUT_ELEMENT_DESC                    vsInputsStk[16]={};
  std::unique_ptr<D3D12_INPUT_ELEMENT_DESC[]> vsInputHeap;
  D3D12_INPUT_ELEMENT_DESC*                   vsInput = vsInputsStk;
  if(declSize>16) {
    vsInputHeap.reset(new D3D12_INPUT_ELEMENT_DESC[declSize]);
    vsInput = vsInputHeap.get();
    }
  uint32_t offset=0;
  for(size_t i=0;i<declSize;++i){
    auto& loc=vsInput[i];
    loc.SemanticName         = "TEXCOORD"; // spirv cross compiles everything as TEXCOORD
    loc.SemanticIndex        = UINT(i);
    loc.Format               = vertFormats[decl[i]];
    loc.InputSlot            = 0;
    loc.AlignedByteOffset    = offset;
    loc.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    loc.InstanceDataStepRate = 0;

    offset+=vertSize[decl[i]];
    }

  static const D3D12_COMPARISON_FUNC depthFn[size_t(RenderState::ZTestMode::Count)]= {
    D3D12_COMPARISON_FUNC_ALWAYS,
    D3D12_COMPARISON_FUNC_NEVER,
    D3D12_COMPARISON_FUNC_GREATER,
    D3D12_COMPARISON_FUNC_LESS,
    D3D12_COMPARISON_FUNC_GREATER_EQUAL,
    D3D12_COMPARISON_FUNC_LESS_EQUAL,
    D3D12_COMPARISON_FUNC_NOT_EQUAL,
    D3D12_COMPARISON_FUNC_EQUAL,
    };

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout     = { vsInput, UINT(declSize) };
  psoDesc.pRootSignature  = ulay.impl.get();
  psoDesc.VS              = vert.bytecode();
  psoDesc.PS              = frag.bytecode();

  psoDesc.RasterizerState = getRaster(st);
  psoDesc.BlendState      = getBlend(st);
  psoDesc.DepthStencilState.DepthEnable    = st.zTestMode()!=RenderState::ZTestMode::Always ? TRUE : FALSE;
  psoDesc.DepthStencilState.DepthWriteMask = st.isZWriteEnabled() ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
  psoDesc.DepthStencilState.DepthFunc      = depthFn[size_t(st.zTestMode())];
  psoDesc.DepthStencilState.StencilEnable  = FALSE;

  psoDesc.SampleMask            = UINT_MAX;
  psoDesc.SampleDesc.Quality    = 0;
  psoDesc.SampleDesc.Count      = 1;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  if(tp==Triangles)
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; else
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

  psoDesc.NumRenderTargets = 1; // TODO: mrt
  psoDesc.RTVFormats[0]    = DXGI_FORMAT_B8G8R8A8_UNORM; // TODO: pipeline instances

  dxAssert(device.device->CreateGraphicsPipelineState(&psoDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&impl)));

  // Create an empty root signature.
  D3D12_ROOT_SIGNATURE_DESC desc={};
  desc.NumParameters     = 0;
  desc.pParameters       = nullptr;
  desc.NumStaticSamplers = 0;
  desc.pStaticSamplers   = nullptr;
  desc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  dxAssert(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                       &signature.get(),
                                       &error.get()));
  dxAssert(device.device->CreateRootSignature(0,
                                              signature->GetBufferPointer(),
                                              signature->GetBufferSize(),
                                              uuid<ID3D12RootSignature>(),
                                              reinterpret_cast<void**>(&emptySign)));
  }

D3D12_BLEND_DESC DxPipeline::getBlend(const RenderState& st) const {
  static const D3D12_BLEND blendMode[size_t(RenderState::BlendMode::Count)] =  {
    D3D12_BLEND_ZERO,
    D3D12_BLEND_ONE,
    D3D12_BLEND_SRC_COLOR,
    D3D12_BLEND_INV_SRC_COLOR,
    D3D12_BLEND_SRC_ALPHA,
    D3D12_BLEND_INV_SRC_ALPHA,
    D3D12_BLEND_DEST_ALPHA,
    D3D12_BLEND_INV_DEST_ALPHA,
    D3D12_BLEND_DEST_COLOR,
    D3D12_BLEND_INV_DEST_COLOR,
    D3D12_BLEND_SRC_ALPHA_SAT,
    };

  D3D12_RENDER_TARGET_BLEND_DESC b;
  b.BlendEnable           = st.hasBlend() ? TRUE : FALSE;
  b.SrcBlend              = blendMode[size_t(st.blendSource())];
  b.DestBlend             = blendMode[size_t(st.blendDest())];
  b.BlendOp               = D3D12_BLEND_OP_ADD;
  b.SrcBlendAlpha         = b.SrcBlend;
  b.DestBlendAlpha        = b.DestBlend;
  b.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
  b.LogicOpEnable         = FALSE;
  b.LogicOp               = D3D12_LOGIC_OP_CLEAR;
  b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  D3D12_BLEND_DESC d={};
  d.AlphaToCoverageEnable  = FALSE;
  d.IndependentBlendEnable = FALSE;
  for(auto& i:d.RenderTarget)
    i = b;

  return d;
  }

D3D12_RASTERIZER_DESC DxPipeline::getRaster(const RenderState& st) const {
  static const D3D12_CULL_MODE cull[size_t(RenderState::CullMode::Count)]={
    D3D12_CULL_MODE_BACK,
    D3D12_CULL_MODE_FRONT,
    D3D12_CULL_MODE_NONE,
  };

  D3D12_RASTERIZER_DESC rd={};
  rd.FillMode = D3D12_FILL_MODE_SOLID;
  rd.CullMode = cull[size_t(st.cullFaceMode())];

  return rd;
  }

#endif
