#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxdescriptorarray.h"

#include <Tempest/PipelineLayout>

#include "dxaccelerationstructure.h"
#include "dxbuffer.h"
#include "dxdevice.h"
#include "dxtexture.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

static int swizzle(ComponentSwizzle cs, int def) {
  switch(cs) {
    case ComponentSwizzle::Identity:
      return def;
    case ComponentSwizzle::R:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
    case ComponentSwizzle::G:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
    case ComponentSwizzle::B:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
    case ComponentSwizzle::A:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
    //case ComponentSwizzle::One:
    //  return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
    }
  return def;
  }

static UINT compMapping(ComponentMapping mapping) {
  // Describe and create a SRV for the texture.
  int mapv[4] = { swizzle(mapping.r,0),
                  swizzle(mapping.g,1),
                  swizzle(mapping.b,2),
                  swizzle(mapping.a,3) };
  return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(mapv[0],mapv[1],mapv[2],mapv[3]);
  }

DxDescriptorArray::DxDescriptorArray(DxPipelineLay& vlay)
  : lay(&vlay), uav(vlay.lay.size()) {
  val     = lay.handler->allocDescriptors();
  heapCnt = UINT(vlay.heaps.size());
  }

DxDescriptorArray::DxDescriptorArray(DxDescriptorArray&& other)
  : lay(other.lay), uav(lay.handler->lay.size()), uavUsage(other.uavUsage) {
  val       = other.val;
  heapCnt   = other.heapCnt;
  other.lay = DSharedPtr<DxPipelineLay*>{};
  for(size_t i=0; i<lay.handler->lay.size(); ++i)
    uav[i] = other.uav[i];
  }

DxDescriptorArray::~DxDescriptorArray() {
  if(lay)
    lay.handler->freeDescriptors(val);
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture* tex, const Sampler& smp, uint32_t mipLevel) {
  auto& t = *reinterpret_cast<DxTexture*>(tex);

  set(id, &tex, 1, smp, mipLevel);
  uav[id].tex     = tex;
  uavUsage.durty |= (t.nonUniqId!=0);
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer* b, size_t offset) {
  auto&      device = *lay.handler->dev.device;
  DxBuffer&  buf    = *reinterpret_cast<DxBuffer*>(b);
  auto&      prm    = lay.handler->prm[id];

  if(prm.rgnType==D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.Format              = DXGI_FORMAT_R32_TYPELESS;
    desc.ViewDimension       = D3D12_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = UINT(offset/4);
    desc.Buffer.NumElements  = UINT((lay.handler->lay[id].byteSize+3)/4); // UAV size is required to be 4-byte aligned.
    desc.Buffer.Flags        = D3D12_BUFFER_UAV_FLAG_RAW;
    if(desc.Buffer.NumElements==0)
      desc.Buffer.NumElements = buf.sizeInBytes/4;

    auto  gpu = val.cpu[prm.heapId];
    gpu.ptr += prm.heapOffset;

    device.CreateUnorderedAccessView(buf.impl.get(),nullptr,&desc,gpu);
    }
  else if(prm.rgnType==D3D12_DESCRIPTOR_RANGE_TYPE_SRV) {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format                  = DXGI_FORMAT_R32_TYPELESS;
    desc.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Buffer.FirstElement     = UINT(offset/4);
    desc.Buffer.NumElements      = UINT((lay.handler->lay[id].byteSize+3)/4); // SRV size is required to be 4-byte aligned.
    desc.Buffer.Flags            = D3D12_BUFFER_SRV_FLAG_RAW;
    if(desc.Buffer.NumElements==0)
      desc.Buffer.NumElements = buf.sizeInBytes/4;
    auto  gpu = val.cpu[prm.heapId];
    gpu.ptr += prm.heapOffset;
    device.CreateShaderResourceView(buf.impl.get(),&desc,gpu);
    }
  else {
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
    desc.BufferLocation = buf.impl->GetGPUVirtualAddress()+offset;
    desc.SizeInBytes    = UINT(lay.handler->lay[id].byteSize);
    if(desc.SizeInBytes==0)
      desc.SizeInBytes = buf.sizeInBytes;
    desc.SizeInBytes = ((desc.SizeInBytes+255)/256)*256; // CB size is required to be 256-byte aligned.

    auto& prm = lay.handler->prm[id];
    auto  gpu = val.cpu[prm.heapId];
    gpu.ptr += prm.heapOffset;
    device.CreateConstantBufferView(&desc, gpu);
    }

  uav[id].buf    = b;
  uavUsage.durty = true;
  }

void DxDescriptorArray::set(size_t id, const Sampler& smp) {
  auto& device = *lay.handler->dev.device;
  auto& prm    = lay.handler->prm[id];

  UINT filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  D3D12_SAMPLER_DESC smpDesc = {};
  smpDesc.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
  smpDesc.AddressU         = nativeFormat(smp.uClamp);
  smpDesc.AddressV         = nativeFormat(smp.vClamp);
  smpDesc.AddressW         = nativeFormat(smp.wClamp);
  smpDesc.MipLODBias       = 0;
  smpDesc.MaxAnisotropy    = 1;
  smpDesc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
  smpDesc.BorderColor[0]   = 1;
  smpDesc.BorderColor[1]   = 1;
  smpDesc.BorderColor[2]   = 1;
  smpDesc.BorderColor[3]   = 1;
  smpDesc.MinLOD           = 0.0f;
  smpDesc.MaxLOD           = D3D12_FLOAT32_MAX;

  if(smp.minFilter==Filter::Linear)
    filter |= (1u<<D3D12_MIN_FILTER_SHIFT);
  if(smp.magFilter==Filter::Linear)
    filter |= (1u<<D3D12_MAG_FILTER_SHIFT);
  if(smp.mipFilter==Filter::Linear)
    filter |= (1u<<D3D12_MIP_FILTER_SHIFT);

  if(smp.anisotropic) {
    smpDesc.MaxAnisotropy = 16;
    filter |= D3D12_ANISOTROPIC_FILTERING_BIT;
    }
  smpDesc.Filter = D3D12_FILTER(filter);

  auto gpu = val.cpu[prm.heapIdSmp];
  gpu.ptr += prm.heapOffsetSmp;
  device.CreateSampler(&smpDesc,gpu);
  }

void DxDescriptorArray::setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure* t) {
  auto& device = *lay.handler->dev.device;
  auto& tlas   = *reinterpret_cast<DxAccelerationStructure*>(t);
  auto& prm    = lay.handler->prm[id];

  D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
  desc.Format                  = DXGI_FORMAT_UNKNOWN;
  desc.ViewDimension           = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
  desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  desc.RaytracingAccelerationStructure.Location = tlas.impl.impl->GetGPUVirtualAddress();

  auto  gpu = val.cpu[prm.heapId];
  gpu.ptr += prm.heapOffset;
  device.CreateShaderResourceView(nullptr,&desc,gpu);
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt, const Sampler& smp, uint32_t mipLevel) {
  auto& device = *lay.handler->dev.device;
  auto& prm    = lay.handler->prm[id];
  auto& l      = lay.handler->lay[id];

  if(l.runtimeSized) {
    constexpr uint32_t granularity = DxPipelineLay::MAX_BINDLESS;
    uint32_t rSz = ((cnt+granularity-1u) & (~(granularity-1u)));
    if(runtimeArraySz!=rSz) {
      reallocSet(rSz);
      runtimeArraySz = rSz;
      }
    }

  uint32_t descSize = 0;
  uint32_t smpSize  = 0;

  if(cnt>1) {
    descSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    smpSize  = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

  for(size_t i=0; i<cnt; ++i) {
    auto& t = *reinterpret_cast<DxTexture*>(tex[i]);

    if(prm.rgnType==D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
      if(mipLevel==uint32_t(-1))
        mipLevel = 0;
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
      desc.Format             = t.format;
      desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
      desc.Texture2D.MipSlice = mipLevel;

      auto  gpu = val.cpu[prm.heapId];
      gpu.ptr += (prm.heapOffset + i*descSize);
      device.CreateUnorderedAccessView(t.impl.get(),nullptr,&desc,gpu);
      } else {
      D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
      srvDesc.Shader4ComponentMapping = compMapping(smp.mapping);
      srvDesc.Format                  = t.format;
      srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels     = t.mips;
      if(mipLevel!=uint32_t(-1)) {
        srvDesc.Texture2D.MostDetailedMip = mipLevel;
        srvDesc.Texture2D.MipLevels       = 1;
        }

      if(srvDesc.Format==DXGI_FORMAT_D16_UNORM)
        srvDesc.Format = DXGI_FORMAT_R16_UNORM;
      else if(srvDesc.Format==DXGI_FORMAT_D32_FLOAT)
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
      else if(srvDesc.Format==DXGI_FORMAT_D24_UNORM_S8_UINT)
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

      UINT filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
      D3D12_SAMPLER_DESC smpDesc = {};
      smpDesc.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
      smpDesc.AddressU         = nativeFormat(smp.uClamp);
      smpDesc.AddressV         = nativeFormat(smp.vClamp);
      smpDesc.AddressW         = nativeFormat(smp.wClamp);
      smpDesc.MipLODBias       = 0;
      smpDesc.MaxAnisotropy    = 1;
      smpDesc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
      smpDesc.BorderColor[0]   = 1;
      smpDesc.BorderColor[1]   = 1;
      smpDesc.BorderColor[2]   = 1;
      smpDesc.BorderColor[3]   = 1;
      smpDesc.MinLOD           = 0.0f;
      smpDesc.MaxLOD           = D3D12_FLOAT32_MAX;

      if(smp.minFilter==Filter::Linear)
        filter |= (1u<<D3D12_MIN_FILTER_SHIFT);
      if(smp.magFilter==Filter::Linear)
        filter |= (1u<<D3D12_MAG_FILTER_SHIFT);
      if(smp.mipFilter==Filter::Linear)
        filter |= (1u<<D3D12_MIP_FILTER_SHIFT);

      if(smp.anisotropic) {
        smpDesc.MaxAnisotropy = 16;
        filter |= D3D12_ANISOTROPIC_FILTERING_BIT;
        }
      smpDesc.Filter = D3D12_FILTER(filter);

      auto  gpu = val.cpu[prm.heapId];
      gpu.ptr += (prm.heapOffset + i*descSize);
      device.CreateShaderResourceView(t.impl.get(), &srvDesc, gpu);

      if(l.cls==ShaderReflection::Texture) {
        gpu = val.cpu[prm.heapIdSmp];
        gpu.ptr += (prm.heapOffsetSmp + i*smpSize);
        device.CreateSampler(&smpDesc,gpu);
        }
      }
    }
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer** b, size_t cnt) {
  auto&    device = *lay.handler->dev.device;
  auto&    prm    = lay.handler->prm[id];

  uint32_t descSize = 0;
  if(cnt>1)
    descSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  for(size_t i=0; i<cnt; ++i) {
    auto&  buf    = *reinterpret_cast<DxBuffer*>(b[i]);

    if(prm.rgnType==D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
      desc.Format              = DXGI_FORMAT_R32_TYPELESS;
      desc.ViewDimension       = D3D12_UAV_DIMENSION_BUFFER;
      desc.Buffer.FirstElement = 0;
      desc.Buffer.NumElements  = UINT((lay.handler->lay[id].byteSize+3)/4); // UAV size is required to be 4-byte aligned.
      desc.Buffer.Flags        = D3D12_BUFFER_UAV_FLAG_RAW;
      if(desc.Buffer.NumElements==0)
        desc.Buffer.NumElements = buf.sizeInBytes/4;

      auto  gpu = val.cpu[prm.heapId];
      gpu.ptr += (prm.heapOffset + i*descSize);

      device.CreateUnorderedAccessView(buf.impl.get(),nullptr,&desc,gpu);
      }
    else if(prm.rgnType==D3D12_DESCRIPTOR_RANGE_TYPE_SRV) {
      D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
      desc.Format                  = DXGI_FORMAT_R32_TYPELESS;
      desc.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER;
      desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      desc.Buffer.FirstElement     = 0;
      desc.Buffer.NumElements      = UINT((lay.handler->lay[id].byteSize+3)/4); // SRV size is required to be 4-byte aligned.
      desc.Buffer.Flags            = D3D12_BUFFER_SRV_FLAG_RAW;
      if(desc.Buffer.NumElements==0)
        desc.Buffer.NumElements = buf.sizeInBytes/4;
      auto  gpu = val.cpu[prm.heapId];
      gpu.ptr += (prm.heapOffset + i*descSize);
      device.CreateShaderResourceView(buf.impl.get(),&desc,gpu);
      }
    else {
      D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
      desc.BufferLocation = buf.impl->GetGPUVirtualAddress();
      desc.SizeInBytes    = UINT(lay.handler->lay[id].byteSize);
      if(desc.SizeInBytes==0)
        desc.SizeInBytes = buf.sizeInBytes;
      desc.SizeInBytes = ((desc.SizeInBytes+255)/256)*256; // CB size is required to be 256-byte aligned.

      auto& prm = lay.handler->prm[id];
      auto  gpu = val.cpu[prm.heapId];
      gpu.ptr += (prm.heapOffset + i*descSize);
      device.CreateConstantBufferView(&desc, gpu);
      }
    }
  }

void DxDescriptorArray::ssboBarriers(ResourceState& res, PipelineStage st) {
  auto& lay = this->lay.handler->lay;
  if(T_UNLIKELY(uavUsage.durty)) {
    uavUsage.read  = NonUniqResId::I_None;
    uavUsage.write = NonUniqResId::I_None;
    for(size_t i=0; i<lay.size(); ++i) {
      NonUniqResId id = NonUniqResId::I_None;
      if(uav[i].buf!=nullptr)
        id = reinterpret_cast<DxBuffer*>(uav[i].buf)->nonUniqId;
      if(uav[i].tex!=nullptr)
        id = reinterpret_cast<DxTexture*>(uav[i].tex)->nonUniqId;

      uavUsage.read |= id;
      if(lay[i].cls==ShaderReflection::ImgRW || lay[i].cls==ShaderReflection::SsboRW)
        uavUsage.write |= id;
      }
    uavUsage.durty = false;
    }
  res.onUavUsage(uavUsage,st);
  }

void DxDescriptorArray::reallocSet(uint32_t newRuntimeSz) {

  }

#endif
