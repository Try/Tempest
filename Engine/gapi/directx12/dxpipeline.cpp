#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxpipeline.h"
#include "dxdevice.h"
#include "dxshader.h"
#include "dxpipelinelay.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

struct D3DX12_MESH_SHADER_PIPELINE_STATE_DESC {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type0 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS;
  D3D12_PIPELINE_STATE_FLAGS    Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type1 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK;
  UINT                          NodeMask = 0;

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type2 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
  ID3D12RootSignature*          pRootSignature = nullptr;

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type3 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
  D3D12_SHADER_BYTECODE         PS = {};

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type4 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS;
  D3D12_SHADER_BYTECODE         AS = {};

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type5 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS;
  D3D12_SHADER_BYTECODE         MS = {};

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type6 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND;
  D3D12_BLEND_DESC              BlendState = {};
  uint32_t                      padd0 = 0;

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type7 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1;
  D3D12_DEPTH_STENCIL_DESC1     DepthStencilState = {};
  uint32_t                      padd1 = 0;

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type8 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;
  DXGI_FORMAT                   DSVFormat = DXGI_FORMAT_UNKNOWN;

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type9 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER;
  D3D12_RASTERIZER_DESC         RasterizerState = {};

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type10 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
  D3D12_RT_FORMAT_ARRAY         RTVFormats = {};

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type11 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC;
  DXGI_SAMPLE_DESC              SampleDesc = {};
  uint32_t                      padd2 = 0;

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type12 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK;
  UINT                          SampleMask = 0;

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type13 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO;
  D3D12_CACHED_PIPELINE_STATE   CachedPSO = {};

  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type14 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING;
  D3D12_VIEW_INSTANCING_DESC    ViewInstancingDesc = {};
  };

DxPipeline::DxPipeline(DxDevice& device,
                       const RenderState& st, Topology tp, const DxPipelineLay& ulay,
                       const DxShader*const* sh, size_t count)
  : sign(ulay.impl.get()), layout(ulay), device(device), rState(st) {
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
    declSize = UINT(vert->vert.decl.size());
    vsInput.reset(new D3D12_INPUT_ELEMENT_DESC[declSize]);
    uint32_t offset=0;
    for(size_t i=0;i<declSize;++i){
      auto& loc=vsInput[i];
      loc.SemanticName         = "TEXCOORD"; // spirv cross compiles everything as TEXCOORD
      loc.SemanticIndex        = UINT(i);
      loc.Format               = nativeFormat(vert->vert.decl[i]);
      loc.InputSlot            = 0;
      loc.AlignedByteOffset    = offset;
      loc.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      loc.InstanceDataStepRate = 0;

      offset += uint32_t(Decl::size(vert->vert.decl[i]));
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
  if(auto sh = findShader(ShaderReflection::Mesh))
    pso.impl = initMeshPipeline(frm); else
    pso.impl = initGraphicsPipeline(frm);

  inst.emplace_back(std::move(pso));
  return *inst.back().impl.get();
  }

IVec3 DxPipeline::workGroupSize() const {
  return wgSize;
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
  b.SrcBlendAlpha         = nativeABlendFormat(st.blendSource());
  b.DestBlendAlpha        = nativeABlendFormat(st.blendDest());
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

D3D12_DEPTH_STENCIL_DESC DxPipeline::getDepth(const RenderState& st, DXGI_FORMAT depthFrm) const {
  D3D12_DEPTH_STENCIL_DESC1 ds1 = getDepth1(st, depthFrm);
  D3D12_DEPTH_STENCIL_DESC  ds  = {};
  std::memcpy(&ds, &ds1, sizeof(ds)); // binary compatible
  return ds;
  }

D3D12_DEPTH_STENCIL_DESC1 DxPipeline::getDepth1(const RenderState& st, DXGI_FORMAT depthFrm) const {
  D3D12_DEPTH_STENCIL_DESC1 ds = {};
  if(depthFrm==DXGI_FORMAT_UNKNOWN) {
    ds.DepthEnable = FALSE;
    return ds;
    }

  ds.DepthEnable    = rState.zTestMode()!=RenderState::ZTestMode::Always ? TRUE : FALSE;
  ds.DepthWriteMask = rState.isZWriteEnabled() ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
  ds.DepthFunc      = nativeFormat(rState.zTestMode());
  ds.StencilEnable  = FALSE;
  if(ds.DepthWriteMask!=D3D12_DEPTH_WRITE_MASK_ZERO && ds.DepthEnable==FALSE) {
    // Testing: DX seem to behave same as Vulkan, while been undocumented
    ds.DepthEnable = TRUE;
    ds.DepthFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
    }
  return ds;
  }

ComPtr<ID3D12PipelineState> DxPipeline::initGraphicsPipeline(const DxFboLayout& frm) {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout     = { vsInput.get(), UINT(declSize) };
  psoDesc.pRootSignature  = sign.get();

  if(auto sh = findShader(ShaderReflection::Vertex))
    psoDesc.VS = sh->bytecode();
  if(auto sh = findShader(ShaderReflection::Fragment))
    psoDesc.PS = sh->bytecode();

  psoDesc.RasterizerState    = getRaster(rState);
  psoDesc.BlendState         = getBlend (rState);
  psoDesc.DepthStencilState  = getDepth (rState, frm.DSVFormat);
  psoDesc.SampleMask         = UINT_MAX;
  psoDesc.SampleDesc.Quality = 0;
  psoDesc.SampleDesc.Count   = 1;
  psoDesc.DSVFormat          = frm.DSVFormat;
  psoDesc.NumRenderTargets   = frm.NumRenderTargets;
  for(uint8_t i=0;i<frm.NumRenderTargets;++i)
    psoDesc.RTVFormats[i] = frm.RTVFormats[i];

  switch(topology) {
    case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
      break;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
      break;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      break;
    case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
    default:
      assert(0);
    }

  ComPtr<ID3D12PipelineState> ret;
  const auto err = device.device->CreateGraphicsPipelineState(&psoDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&ret.get()));
  if(FAILED(err)) {
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  return ret;
  }


ComPtr<ID3D12PipelineState> DxPipeline::initMeshPipeline(const DxFboLayout& frm) {
  D3DX12_MESH_SHADER_PIPELINE_STATE_DESC psoDesc;
  psoDesc.pRootSignature  = sign.get();
  if(auto sh = findShader(ShaderReflection::Task))
    psoDesc.AS = sh->bytecode();
  if(auto sh = findShader(ShaderReflection::Mesh))
    psoDesc.MS = sh->bytecode();
  if(auto sh = findShader(ShaderReflection::Fragment))
    psoDesc.PS = sh->bytecode();

  if(auto task = findShader(ShaderReflection::Task)) {
    wgSize = task->comp.wgSize;
    }
  else if(auto mesh = findShader(ShaderReflection::Mesh)) {
    wgSize = mesh->comp.wgSize;
    }

  //int x = offsetof(D3DX12_MESH_SHADER_PIPELINE_STATE_DESC, D3DX12_MESH_SHADER_PIPELINE_STATE_DESC::RasterizerState)      - 4;
  //int y = offsetof(D3DX12_MESH_SHADER_PIPELINE_STATE_DESC, D3DX12_MESH_SHADER_PIPELINE_STATE_DESC::SampleMask) - 4;

  psoDesc.BlendState        = getBlend (rState);
  psoDesc.SampleMask        = UINT_MAX;
  psoDesc.RasterizerState   = getRaster(rState);
  psoDesc.DepthStencilState = getDepth1(rState, frm.DSVFormat);

  psoDesc.DSVFormat = frm.DSVFormat;
  psoDesc.RTVFormats.NumRenderTargets = frm.NumRenderTargets;
  for(uint8_t i=0;i<frm.NumRenderTargets;++i)
    psoDesc.RTVFormats.RTFormats[i] = frm.RTVFormats[i];
  psoDesc.SampleDesc.Quality    = 0;
  psoDesc.SampleDesc.Count      = 1;

  ComPtr<ID3D12PipelineState> ret;
  //auto err = device.device->CreateGraphicsPipelineState(&psoDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&ret.get()));
  // Populate the stream desc with our defined PSO descriptor
  D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = {};
  streamDesc.SizeInBytes                   = sizeof(psoDesc);
  streamDesc.pPipelineStateSubobjectStream = &psoDesc;

  ComPtr<ID3D12Device2> dev;
  device.device->QueryInterface(uuid<ID3D12Device2>(), reinterpret_cast<void**>(&dev));

  const auto err = dev->CreatePipelineState(&streamDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&ret.get()));
  if(FAILED(err)) {
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  return ret;
  }


DxCompPipeline::DxCompPipeline(DxDevice& device, const DxPipelineLay& ulay, DxShader& comp)
  : sign(ulay.impl.get()), layout(ulay) {
  sign.get()->AddRef();

  wgSize         = comp.comp.wgSize;
  pushConstantId = ulay.pushConstantId;

  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = sign.get();
  psoDesc.CS             = comp.bytecode();

  const auto err = device.device->CreateComputePipelineState(&psoDesc, uuid<ID3D12PipelineState>(), reinterpret_cast<void**>(&impl));
  if(FAILED(err)) {
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule, "\"" + comp.dbg.source + "\"");
    }
  }

IVec3 DxCompPipeline::workGroupSize() const {
  return wgSize;
  }

#endif

