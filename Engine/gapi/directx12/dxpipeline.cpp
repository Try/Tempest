#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxpipeline.h"

#include "dxdevice.h"
#include "dxframebuffer.h"
#include "dxshader.h"
#include "dxuniformslay.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

bool DxPipeline::Inst::isCompatible(const DxFramebuffer& frm) const {
  if(viewsCount!=frm.viewsCount)
    return false;
  for(uint8_t i=0;i<viewsCount;++i)
    if(RTVFormats[i]!=frm.views[i].format)
      return false;
  return true;
  }

DxPipeline::DxPipeline(DxDevice& device, const RenderState& st,
                       const Decl::ComponentType* decl, size_t declSize, size_t stride,
                       Topology tp, const DxUniformsLay& ulay,
                       DxShader& vert, DxShader& frag)
  : sign(ulay.impl.get()), stride(UINT(stride)),
    device(device),
    vsShader(&vert), fsShader(&frag), declSize(UINT(declSize)), rState(st) {
  sign.get()->AddRef();
  static const D3D_PRIMITIVE_TOPOLOGY dxTopolgy[]= {
    D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,
    D3D_PRIMITIVE_TOPOLOGY_LINELIST,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    };
  topology = dxTopolgy[int(tp)];

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

  vsInput.reset(new D3D12_INPUT_ELEMENT_DESC[declSize]);

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
  }

ID3D12PipelineState& DxPipeline::instance(const DxFramebuffer& frm) {
  std::lock_guard<SpinLock> guard(sync);

  for(auto& i:inst) {
    if(i.isCompatible(frm))
      return *i.impl.get();
    }

  Inst pso;
  pso.impl = initGraphicsPipeline(frm);
  pso.viewsCount = frm.viewsCount;
  for(uint8_t i=0;i<frm.viewsCount;++i)
    pso.RTVFormats[i] = frm.views[i].format;
  inst.emplace_back(std::move(pso));
  return *inst.back().impl.get();
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

  D3D12_RASTERIZER_DESC rd = {};
  rd.FillMode              = D3D12_FILL_MODE_SOLID;
  rd.CullMode              = cull[size_t(st.cullFaceMode())];
  rd.FrontCounterClockwise = TRUE;

  return rd;
  }

ComPtr<ID3D12PipelineState> DxPipeline::initGraphicsPipeline(const DxFramebuffer& frm) {
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
  psoDesc.InputLayout     = { vsInput.get(), UINT(declSize) };
  psoDesc.pRootSignature  = sign.get();
  psoDesc.VS              = vsShader.handler->bytecode();
  psoDesc.PS              = fsShader.handler->bytecode();

  psoDesc.RasterizerState = getRaster(rState);
  psoDesc.BlendState      = getBlend (rState);
  psoDesc.DepthStencilState.DepthEnable    = rState.zTestMode()!=RenderState::ZTestMode::Always ? TRUE : FALSE;
  psoDesc.DepthStencilState.DepthWriteMask = rState.isZWriteEnabled() ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
  psoDesc.DepthStencilState.DepthFunc      = depthFn[size_t(rState.zTestMode())];
  psoDesc.DepthStencilState.StencilEnable  = FALSE;

  psoDesc.SampleMask            = UINT_MAX;
  psoDesc.SampleDesc.Quality    = 0;
  psoDesc.SampleDesc.Count      = 1;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  if(topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST ||
     topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP ||
     topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ ||
     topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; else
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

  //psoDesc.DSVFormat; // TODO: depth-stencil
  psoDesc.NumRenderTargets = frm.viewsCount;
  for(uint8_t i=0;i<frm.viewsCount;++i)
    psoDesc.RTVFormats[i] = frm.views[i].format;

  ComPtr<ID3D12PipelineState> ret;
  dxAssert(device.device->CreateGraphicsPipelineState(&psoDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&ret)));
  return ret;
  }

#endif
