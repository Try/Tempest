#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxpipelinelay.h"

#include "dxdevice.h"
#include "dxshader.h"
#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

static D3D12_DESCRIPTOR_RANGE_TYPE nativeFormat(ShaderReflection::Class cls) {
  switch(cls) {
    case ShaderReflection::Ubo:
      return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case ShaderReflection::Texture:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // NOTE:combined
    case ShaderReflection::Image:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ShaderReflection::Sampler:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    case ShaderReflection::SsboR:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ShaderReflection::SsboRW:
      return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case ShaderReflection::ImgR:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ShaderReflection::ImgRW:
      return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case ShaderReflection::Tlas:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ShaderReflection::Push:
    case ShaderReflection::Count:
      assert(0);
    }
  return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  }

static D3D12_SHADER_VISIBILITY nativeFormat(ShaderReflection::Stage stage) {
  switch(stage) {
    case ShaderReflection::Stage::None:
      return D3D12_SHADER_VISIBILITY_ALL;
    case ShaderReflection::Stage::Vertex:
      return D3D12_SHADER_VISIBILITY_VERTEX;
    case ShaderReflection::Stage::Control:
      return D3D12_SHADER_VISIBILITY_HULL;
    case ShaderReflection::Stage::Evaluate:
      return D3D12_SHADER_VISIBILITY_DOMAIN;
    case ShaderReflection::Stage::Fragment:
      return D3D12_SHADER_VISIBILITY_PIXEL;
    case ShaderReflection::Stage::Geometry:
      return D3D12_SHADER_VISIBILITY_GEOMETRY;
    case ShaderReflection::Stage::Mesh:
      return D3D12_SHADER_VISIBILITY_MESH;
    case ShaderReflection::Stage::Task:
      return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
    case ShaderReflection::Stage::Compute:
      return D3D12_SHADER_VISIBILITY_ALL;
    }
  return D3D12_SHADER_VISIBILITY_ALL;
  }

DxPipelineLay::DxPipelineLay(DxDevice& dev, const std::vector<ShaderReflection::Binding>* sh)
  :DxPipelineLay(dev,&sh,1,false) {
  }

DxPipelineLay::DxPipelineLay(DxDevice& dev, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt,
                             bool has_baseVertex_baseInstance) {
  ShaderReflection::setupLayout(pb, layout, sync, sh, cnt);

  auto& device = *dev.device;
  std::vector<D3D12_DESCRIPTOR_RANGE> rgn;
  // shader-resources
  const size_t resBg  = rgn.size();
  {
    uint32_t offset = 0;
    for(size_t i=0; i<MaxBindings; ++i) {
      if(((1u << i) & layout.active)==0)
        continue;
      if(((1u << i) & layout.array)!=0)
        continue;
      if(layout.bindings[i]==ShaderReflection::Sampler)
        continue;
      D3D12_DESCRIPTOR_RANGE rx = {};
      rx.RangeType          = ::nativeFormat(layout.bindings[i]);
      rx.NumDescriptors     = 1;
      rx.BaseShaderRegister = UINT(i);
      rx.RegisterSpace      = 0;
      rx.OffsetInDescriptorsFromTableStart = UINT(offset);
      ++offset;
      rgn.push_back(rx);
      }
  }
  const size_t numRes = rgn.size() - resBg;

  // samplers
  const size_t smpBg  = rgn.size();
  {
    uint32_t offset = 0;
    for(size_t i=0; i<MaxBindings; ++i) {
      if(layout.bindings[i]!=ShaderReflection::Sampler && layout.bindings[i]!=ShaderReflection::Texture)
        continue;
      if(((1u << i) & layout.array)!=0)
        continue;
      D3D12_DESCRIPTOR_RANGE rx = {};
      rx.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
      rx.NumDescriptors     = 1;
      rx.BaseShaderRegister = UINT(i);
      rx.RegisterSpace      = 0;
      rx.OffsetInDescriptorsFromTableStart = UINT(offset);
      ++offset;
      rgn.push_back(rx);
      }
  }
  const size_t numSmp = rgn.size() - smpBg;

  // arrays
  const size_t arrBg  = rgn.size();
  {
    for(size_t i=0; i<MaxBindings; ++i) {
      if(((1u << i) & layout.active)==0)
        continue;
      if(((1u << i) & layout.array)==0)
        continue;
      D3D12_DESCRIPTOR_RANGE rx = {};
      rx.RangeType                         = ::nativeFormat(layout.bindings[i]);
      rx.NumDescriptors                    = -1;
      rx.BaseShaderRegister                = 0;
      rx.RegisterSpace                     = UINT(i+1);
      rx.OffsetInDescriptorsFromTableStart = 0;
      rgn.push_back(rx);
      if(layout.bindings[i]==ShaderReflection::Texture) {
        rx.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        rgn.push_back(rx);
        }
      }
  }
  const size_t numArr = rgn.size() - arrBg;

  std::vector<D3D12_ROOT_PARAMETER> rootPrm;
  if(numRes>0) {
    D3D12_ROOT_PARAMETER prm = {};
    prm.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    prm.DescriptorTable.NumDescriptorRanges = UINT(numRes);
    prm.DescriptorTable.pDescriptorRanges   = rgn.data() + resBg;
    prm.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
    rootPrm.push_back(prm);
    }
  if(numSmp>0) {
    D3D12_ROOT_PARAMETER prm = {};
    prm.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    prm.DescriptorTable.NumDescriptorRanges = UINT(numSmp);
    prm.DescriptorTable.pDescriptorRanges   = rgn.data() + smpBg;
    prm.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
    rootPrm.push_back(prm);
    }
  for(size_t i=0; i<numArr; ++i) {
    D3D12_ROOT_PARAMETER prm = {};
    prm.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    prm.DescriptorTable.NumDescriptorRanges = 1;
    prm.DescriptorTable.pDescriptorRanges   = rgn.data() + arrBg + i;
    prm.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
    rootPrm.push_back(prm);
    }
  // push
  if(pb.size>0) {
    D3D12_ROOT_PARAMETER prmPush = {};
    prmPush.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    prmPush.ShaderVisibility         = ::nativeFormat(pb.stage);
    prmPush.Constants.ShaderRegister = DxShader::HLSL_PUSH;
    prmPush.Constants.RegisterSpace  = 0;
    prmPush.Constants.Num32BitValues = UINT((pb.size+3)/4);
    pushConstantId = uint32_t(rootPrm.size());
    rootPrm.push_back(prmPush);
    }
  // internal
  if(has_baseVertex_baseInstance) {
    D3D12_ROOT_PARAMETER prmPush = {};
    prmPush.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    prmPush.ShaderVisibility         = D3D12_SHADER_VISIBILITY_VERTEX;
    prmPush.Constants.ShaderRegister = DxShader::HLSL_BASE_VERTEX_INSTANCE;
    prmPush.Constants.RegisterSpace  = 0;
    prmPush.Constants.Num32BitValues = 2u;
    pushBaseInstanceId = uint32_t(rootPrm.size());
    rootPrm.push_back(prmPush);
    }

  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc={};
  rootSignatureDesc.NumParameters     = UINT(rootPrm.size());
  rootSignatureDesc.pParameters       = rootPrm.data();
  rootSignatureDesc.NumStaticSamplers = 0;
  rootSignatureDesc.pStaticSamplers   = nullptr;
  rootSignatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;

  auto hr = dev.dllApi.D3D12SerializeRootSignature(&rootSignatureDesc, featureData.HighestVersion,
                                                   &signature.get(), &error.get());
  if(FAILED(hr)) {
#if !defined(NDEBUG)
    const char* msg = reinterpret_cast<const char*>(error->GetBufferPointer());
    Log::e(msg);
#endif
    dxAssert(hr);
    }
  dxAssert(device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                      uuid<ID3D12RootSignature>(), reinterpret_cast<void**>(&impl)));
  }

#endif

