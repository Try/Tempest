#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxpipeline.h"

#include "dxdevice.h"
#include "dxshader.h"
#include "dxpipelinelay.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxPipeline::DxPipeline(DxDevice& device,
                       const RenderState& st, Topology tp, const DxPipelineLay& ulay,
                       const DxShader*const* sh, size_t count)
  : sign(ulay.impl.get()), device(device), rState(st) {
  sign.get()->AddRef();
  for(size_t i=0; i<count; ++i)
    if(sh[i]!=nullptr)
      modules[i] = Detail::DSharedPtr<const DxShader*>{sh[i]};

  topology = nativeFormat(tp);
  if(findShader(ShaderReflection::Control)!=nullptr || findShader(ShaderReflection::Evaluate)!=nullptr) {
    if(tp==Triangles)
      topology = D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST; else
      topology = D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;
    }
  pushConstantId     = ulay.pushConstantId;
  pushBaseInstanceId = ulay.pushBaseInstanceId;

  if(auto vert = findShader(ShaderReflection::Vertex)) {
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

const DxShader* DxPipeline::findShader(ShaderReflection::Stage sh) const {
  for(auto& i:modules)
    if(i.handler!=nullptr && i.handler->stage==sh)
      return i.handler;
  return nullptr;
  }

D3D12_BLEND_DESC DxPipeline::getBlend(const RenderState& st) const {
  D3D12_RENDER_TARGET_BLEND_DESC b;
  b.BlendEnable           = st.hasBlend() ? TRUE : FALSE;
  b.SrcBlend              = nativeFormat(st.blendSource());
  b.DestBlend             = nativeFormat(st.blendDest());
  b.BlendOp               = nativeFormat(st.blendOperation());
  b.SrcBlendAlpha         = b.SrcBlend;
  b.DestBlendAlpha        = b.DestBlend;
  b.BlendOpAlpha          = b.BlendOp;
  b.LogicOpEnable         = FALSE;
  b.LogicOp               = D3D12_LOGIC_OP_CLEAR;
  b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  D3D12_BLEND_DESC d = {};
  d.AlphaToCoverageEnable  = FALSE;
  d.IndependentBlendEnable = FALSE;
  for(auto& i:d.RenderTarget)
    i = b;

  return d;
  }

D3D12_RASTERIZER_DESC DxPipeline::getRaster(const RenderState& st) const {
  D3D12_RASTERIZER_DESC rd = {};
  rd.FillMode              = D3D12_FILL_MODE_SOLID;
  //rd.FillMode              = D3D12_FILL_MODE_WIREFRAME;
  rd.CullMode              = nativeFormat(st.cullFaceMode());
  rd.FrontCounterClockwise = FALSE;
  return rd;
  }

ComPtr<ID3D12PipelineState> DxPipeline::initGraphicsPipeline(const DxFboLayout& frm) {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout     = { vsInput.get(), UINT(declSize) };
  psoDesc.pRootSignature  = sign.get();
  if(auto sh = findShader(ShaderReflection::Control))
    psoDesc.HS = sh->bytecode();
  if(auto sh = findShader(ShaderReflection::Evaluate))
    psoDesc.DS = sh->bytecode();
  if(auto sh = findShader(ShaderReflection::Geometry))
    psoDesc.GS = sh->bytecode();
  if(auto sh = findShader(ShaderReflection::Fragment))
    psoDesc.PS = sh->bytecode();

  const bool useTesselation = (psoDesc.HS.pShaderBytecode!=nullptr || psoDesc.DS.pShaderBytecode!=nullptr);
  if(auto sh = findShader(ShaderReflection::Vertex)) {
    if(useTesselation)
      psoDesc.VS = sh->bytecode(DxShader::Flavor::NoFlipY); else
      psoDesc.VS = sh->bytecode();
    }

  psoDesc.RasterizerState = getRaster(rState);
  psoDesc.BlendState      = getBlend (rState);
  psoDesc.DepthStencilState.DepthEnable    = rState.zTestMode()!=RenderState::ZTestMode::Always ? TRUE : FALSE;
  psoDesc.DepthStencilState.DepthWriteMask = rState.isZWriteEnabled() ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
  psoDesc.DepthStencilState.DepthFunc      = nativeFormat(rState.zTestMode());
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

  if(useTesselation) {
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    }

  ComPtr<ID3D12PipelineState> ret;
  auto err = device.device->CreateGraphicsPipelineState(&psoDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&ret.get()));
  if(FAILED(err)) {
    for(auto& i:modules)
      if(i.handler!=nullptr)
        i.handler->disasm();
    dxAssert(err);
    }
  return ret;
  }


DxCompPipeline::DxCompPipeline(DxDevice& device, const DxPipelineLay& ulay, DxShader& comp)
  : sign(ulay.impl.get()) {
  sign.get()->AddRef();

  wgSize         = comp.comp.wgSize;
  pushConstantId = ulay.pushConstantId;

  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = sign.get();
  psoDesc.CS             = comp.bytecode();

  dxAssert(device.device->CreateComputePipelineState(&psoDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&impl)));
  }

IVec3 DxCompPipeline::workGroupSize() const {
  return wgSize;
  }

#endif

