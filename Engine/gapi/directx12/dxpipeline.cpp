#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxpipeline.h"

#include "dxdevice.h"
#include "dxshader.h"
#include "dxpipelinelay.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxPipeline::DxPipeline(DxDevice& device,
                       const RenderState& st, size_t stride, Topology tp, const DxPipelineLay& ulay,
                       const DxShader* vert, const DxShader* ctrl, const DxShader* tess, const DxShader* geom, const DxShader* frag)
  : sign(ulay.impl.get()), stride(UINT(stride)),
    device(device),
    vsShader(vert), tcShader(ctrl), teShader(tess), gsShader(geom), fsShader(frag), rState(st) {
  sign.get()->AddRef();
  static const D3D_PRIMITIVE_TOPOLOGY dxTopolgy[]= {
    D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,
    D3D_PRIMITIVE_TOPOLOGY_LINELIST,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    };
  topology       = dxTopolgy[int(tp)];
  pushConstantId = ulay.pushConstantId;
  ssboBarriers   = ulay.hasSsbo;

  if(vert!=nullptr) {
    declSize = UINT(vert->vdecl.size());
    vsInput.reset(new D3D12_INPUT_ELEMENT_DESC[declSize]);
    uint32_t offset=0;
    for(size_t i=0;i<declSize;++i){
      auto& loc=vsInput[i];
      loc.SemanticName         = "TEXCOORD"; // spirv cross compiles everything as TEXCOORD
      loc.SemanticIndex        = UINT(i);
      loc.Format               = nativeFormat(vert->vdecl[i]);
      loc.InputSlot            = 0;
      loc.AlignedByteOffset    = offset;
      loc.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      loc.InstanceDataStepRate = 0;

      offset += uint32_t(Decl::size(vert->vdecl[i]));
      }
    }
  }

ID3D12PipelineState& DxPipeline::instance(DXGI_FORMAT frm) {
  DxFboLayout lay;
  lay.RTVFormats[0]    = frm;
  lay.NumRenderTargets = 1;
  return instance(lay);
  }

ID3D12PipelineState& DxPipeline::instance(const DxFboLayout& frm) {
  std::lock_guard<SpinLock> guard(sync);

  for(auto& i:inst) {
    if(i.lay.equals(frm))
      return *i.impl.get();
    }

  Inst pso;
  pso.lay  = frm;
  pso.impl = initGraphicsPipeline(frm);
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

  static const D3D12_BLEND_OP blendOp[] = {
    D3D12_BLEND_OP_ADD,
    D3D12_BLEND_OP_SUBTRACT,
    D3D12_BLEND_OP_REV_SUBTRACT,
    D3D12_BLEND_OP_MIN,
    D3D12_BLEND_OP_MAX,
    };

  D3D12_RENDER_TARGET_BLEND_DESC b;
  b.BlendEnable           = st.hasBlend() ? TRUE : FALSE;
  b.SrcBlend              = blendMode[uint8_t(st.blendSource())];
  b.DestBlend             = blendMode[uint8_t(st.blendDest())];
  b.BlendOp               = blendOp[uint8_t(st.blendOperation())];
  b.SrcBlendAlpha         = b.SrcBlend;
  b.DestBlendAlpha        = b.DestBlend;
  b.BlendOpAlpha          = b.BlendOp;
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
  rd.FrontCounterClockwise = FALSE;

  return rd;
  }

ComPtr<ID3D12PipelineState> DxPipeline::initGraphicsPipeline(const DxFboLayout& frm) {
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
  if(vsShader.handler)
    psoDesc.VS = vsShader.handler->bytecode();
  if(tcShader.handler)
    psoDesc.HS = tcShader.handler->bytecode();
  if(teShader.handler)
    psoDesc.DS = teShader.handler->bytecode();
  if(gsShader.handler)
    psoDesc.GS = gsShader.handler->bytecode();
  if(fsShader.handler)
    psoDesc.PS = fsShader.handler->bytecode();

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

  psoDesc.DSVFormat = frm.DSVFormat;
  psoDesc.NumRenderTargets = frm.NumRenderTargets;
  for(uint8_t i=0;i<frm.NumRenderTargets;++i)
    psoDesc.RTVFormats[i] = frm.RTVFormats[i];

  ComPtr<ID3D12PipelineState> ret;
  auto err = device.device->CreateGraphicsPipelineState(&psoDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&ret));
  if(FAILED(err)) {
    if(vsShader.handler)
      vsShader.handler->disasm();
    if(fsShader.handler)
      fsShader.handler->disasm();
    dxAssert(err);
    }
  return ret;
  }

DxCompPipeline::DxCompPipeline(DxDevice& device, const DxPipelineLay& ulay, DxShader& comp)
  : sign(ulay.impl.get()) {
  sign.get()->AddRef();

  pushConstantId = ulay.pushConstantId;
  ssboBarriers   = ulay.hasSsbo;

  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = sign.get();
  psoDesc.CS             = comp.bytecode();

  dxAssert(device.device->CreateComputePipelineState(&psoDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&impl)));
  }

#endif

