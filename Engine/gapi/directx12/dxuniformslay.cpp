#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxuniformslay.h"

#include <Tempest/UniformsLayout>

#include "dxdevice.h"
#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

DxUniformsLay::DxUniformsLay(DxDevice& dev, const UniformsLayout& lay)
  : prm(lay.size()){
  auto& device = *dev.device;

  descSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

  D3D12_DESCRIPTOR_RANGE  rgn[VisTypeCount*MaxPrmPerStage] = {};
  D3D12_ROOT_PARAMETER    rootParameters[VisTypeCount*MaxPrmPerStage] = {};
  size_t                  numParameters                               = 0;

  for(size_t i=0;i<lay.size();++i) {
    auto& l = lay[i];
    auto& p = prm[i];

    add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,p,rootParameters,rgn,numParameters);
    if(l.cls==UniformsLayout::Texture)
      add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,p,rootParameters,rgn,numParameters);
    }

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc={};
  rootSignatureDesc.NumParameters     = UINT(numParameters);
  rootSignatureDesc.pParameters       = rootParameters;
  rootSignatureDesc.NumStaticSamplers = 0;
  rootSignatureDesc.pStaticSamplers   = nullptr;
  rootSignatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;

  dxAssert(D3D12SerializeRootSignature(&rootSignatureDesc, featureData.HighestVersion,
                                       &signature.get(), &error.get()));
  dxAssert(device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                      uuid<ID3D12RootSignature>(), reinterpret_cast<void**>(&impl)));

  heaps.resize(numParameters);
  for(size_t i=0;i<numParameters;++i) {
    if(rgn[i].RangeType==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
      heaps[i].type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER; else
      heaps[i].type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heaps[i].numDesc = rgn[i].NumDescriptors;
    }
  }

void DxUniformsLay::add(const UniformsLayout::Binding& b, D3D12_DESCRIPTOR_RANGE_TYPE type,
                        Param& prm,
                        D3D12_ROOT_PARAMETER* root, D3D12_DESCRIPTOR_RANGE* rgn,
                        size_t& cnt) {
  D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
  switch(b.stage) {
    case UniformsLayout::Vertex:   visibility = D3D12_SHADER_VISIBILITY_VERTEX; break;
    case UniformsLayout::Fragment: visibility = D3D12_SHADER_VISIBILITY_PIXEL;  break;
    }

  for(size_t i=0;i<cnt;++i) {
    if(root[i].ShaderVisibility!=visibility)
      continue;
    if(rgn[i].RangeType!=type)
      continue;
    // found
    if(type==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) {
      prm.heapOffsetSmp = rgn[i].NumDescriptors*descSize;
      prm.heapIdSmp     = uint8_t(i);
      } else {
      prm.heapOffset    = rgn[i].NumDescriptors*descSize;
      prm.heapId        = uint8_t(i);
      }
    rgn[i].NumDescriptors++;
    return;
    }

  auto& r = rgn[cnt];
  r.RangeType          = type;
  r.NumDescriptors     = 1;
  r.BaseShaderRegister = 0;
  r.RegisterSpace      = 0;
  r.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  auto& param = root[cnt];
  param.ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.ShaderVisibility = visibility;

  param.DescriptorTable.pDescriptorRanges   = &r;
  param.DescriptorTable.NumDescriptorRanges = 1;

  if(type==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) {
    prm.heapOffsetSmp = 0;
    prm.heapIdSmp     = uint8_t(cnt);
    } else {
    prm.heapOffset    = 0;
    prm.heapId        = uint8_t(cnt);
    }

  cnt++;
  }

#endif
