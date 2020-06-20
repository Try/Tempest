#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxuniformslay.h"

#include <Tempest/UniformsLayout>

#include "gapi/shaderreflection.h"
#include "dxdevice.h"
#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

DxUniformsLay::DxUniformsLay(DxDevice& dev,
                             const std::vector<UniformsLayout::Binding>& vs,
                             const std::vector<UniformsLayout::Binding>& fs) {
  auto& device = *dev.device;
  descSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  ShaderReflection::merge(lay, vs,fs);
  prm.resize(lay.size());

  std::vector<Parameter> desc;
  for(size_t i=0;i<lay.size();++i) {
    auto& l = lay[i];
    switch(l.cls) {
      case UniformsLayout::Ubo: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_CBV,desc);
        break;
        }
      case UniformsLayout::Texture: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,    desc);
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,desc);
        break;
        }
      case UniformsLayout::Push: {
        //TODO
        break;
        }
      }
    }
  std::sort(desc.begin(),desc.end(),[](const Parameter& a, const Parameter& b){
    return std::tie(a.visibility,a.rgn.RangeType)<std::tie(b.visibility,b.rgn.RangeType);
    });

  std::vector<D3D12_DESCRIPTOR_RANGE> rgn(desc.size());
  std::vector<D3D12_ROOT_PARAMETER>   rootPrm;

  uint8_t curVisibility = 255;
  uint8_t curRgnType    = 255;
  for(size_t i=0; i<rgn.size(); ++i) {
    rgn[i] = desc[i].rgn;

    D3D12_DESCRIPTOR_HEAP_TYPE heapType
        = (rgn[i].RangeType==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) ?
          D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER :
          D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    Heap* heap = nullptr;
    for(size_t r=0;r<heaps.size();++r) {
      if(heaps[r].type==heapType) {
        heap = &heaps[r];
        break;
        }
      }

    if(heap==nullptr) {
      heaps.emplace_back();
      heap = &heaps.back();
      heap->type    = heapType;
      heap->numDesc = 0;
      }

    if(uint8_t(desc[i].visibility)   !=curVisibility ||
       uint8_t(desc[i].rgn.RangeType)!=curRgnType) {
      D3D12_ROOT_PARAMETER p = {};
      p.ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      p.ShaderVisibility = desc[i].visibility;
      p.DescriptorTable.pDescriptorRanges   = &rgn[i];
      p.DescriptorTable.NumDescriptorRanges = 0;
      rootPrm.push_back(p);

      RootPrm r = {};
      r.heap       = uint8_t(std::distance(&heaps[0],heap));
      r.heapOffset = heap->numDesc;
      roots.push_back(r);

      curVisibility = uint8_t(desc[i].visibility);
      curRgnType    = uint8_t(desc[i].rgn.RangeType);
      }

    auto& px = rootPrm.back();
    px.DescriptorTable.NumDescriptorRanges++;
    heap->numDesc += rgn[i].NumDescriptors;

    auto& p = prm[desc[i].id];
    if(curRgnType==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) {
      p.heapOffsetSmp = heap->numDesc-1;
      p.heapIdSmp     = uint8_t(std::distance(&heaps[0],heap));
      } else {
      p.heapOffset    = heap->numDesc-1;
      p.heapId        = uint8_t(std::distance(&heaps[0],heap));
      }
    }

  for(auto& i:prm) {
    i.heapOffset    *= descSize;
    i.heapOffsetSmp *= descSize;
    }

  for(auto& i:roots) {
    i.heapOffset *= descSize;
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

  dxAssert(D3D12SerializeRootSignature(&rootSignatureDesc, featureData.HighestVersion,
                                       &signature.get(), &error.get()));
  dxAssert(device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                      uuid<ID3D12RootSignature>(), reinterpret_cast<void**>(&impl)));
/*
  heaps.resize(prm.size());
  for(size_t i=0; i<prm.size(); ++i) {
    if(prm[i].DescriptorTable.pDescriptorRanges[0].RangeType==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
      heaps[i].type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER; else
      heaps[i].type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heaps[i].numDesc = 0;
    for(UINT r=0; r<prm[i].DescriptorTable.NumDescriptorRanges; ++r)
      heaps[i].numDesc+=prm[i].DescriptorTable.pDescriptorRanges[r].NumDescriptors;
    }*/
  }

void Tempest::Detail::DxUniformsLay::add(const Tempest::UniformsLayout::Binding& b,
                                         D3D12_DESCRIPTOR_RANGE_TYPE type,
                                         std::vector<Parameter>& root) {
  D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
  switch(b.stage) {
    case UniformsLayout::Vertex:   visibility = D3D12_SHADER_VISIBILITY_VERTEX; break;
    case UniformsLayout::Fragment: visibility = D3D12_SHADER_VISIBILITY_PIXEL;  break;
    }

  Parameter rp;

  rp.rgn.RangeType          = type;
  rp.rgn.NumDescriptors     = 1;
  rp.rgn.BaseShaderRegister = b.layout;
  rp.rgn.RegisterSpace      = 0;
  rp.rgn.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  rp.id         = b.layout;
  rp.visibility = visibility;

  root.push_back(rp);
  }

#endif
