#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxdescriptorarray.h"

#include <Tempest/PipelineLayout>

#include "dxaccelerationstructure.h"
#include "dxbuffer.h"
#include "dxdevice.h"
#include "dxtexture.h"
#include "guid.h"

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
  heapCnt = 0;
  for(auto& h:vlay.heaps)
    if(h.numDesc>0)
      heapCnt++;

  if(lay.handler->isRuntimeSized()) {
    runtimeArrays.resize(lay.handler->prm.size());
    for(size_t i=0; i<lay.handler->lay.size(); ++i) {
      auto& l = lay.handler->lay[i];
      runtimeArrays[i].size = l.arraySize;
      if(!l.runtimeSized) {
        switch(l.cls) {
          case ShaderReflection::Ubo:
          case ShaderReflection::Texture:
          case ShaderReflection::Image:
          case ShaderReflection::SsboR:
          case ShaderReflection::SsboRW:
          case ShaderReflection::ImgR:
          case ShaderReflection::ImgRW:
          case ShaderReflection::Tlas:
            runtimeArrays[i].data = {nullptr};
            break;
          case ShaderReflection::Sampler:
          case ShaderReflection::Push:
          case ShaderReflection::Count:
            break;
          }
        }
      }
    reallocSet(0, 0);
    } else {
    val = lay.handler->allocDescriptors();
    }
  }

DxDescriptorArray::DxDescriptorArray(DxDescriptorArray&& other)
  : lay(other.lay), uav(lay.handler->lay.size()), uavUsage(other.uavUsage) {
  val       = other.val;
  heapCnt   = other.heapCnt;
  other.lay = DSharedPtr<DxPipelineLay*>{};
  for(size_t i=0; i<lay.handler->lay.size(); ++i)
    uav[i] = other.uav[i];

  for(size_t i=0; i<HEAP_MAX; ++i)
    std::swap(heapDyn[i], other.heapDyn[i]);
  runtimeArrays = std::move(other.runtimeArrays);
  }

DxDescriptorArray::~DxDescriptorArray() {
  if(lay && !lay.handler->isRuntimeSized())
    lay.handler->freeDescriptors(val);

  if(heapDyn[0]!=nullptr)
    heapDyn[0]->Release();
  if(heapDyn[1]!=nullptr)
    heapDyn[1]->Release();
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture* tex, const Sampler& smp, uint32_t mipLevel) {
  auto& t = *reinterpret_cast<DxTexture*>(tex);

  set(id, &tex, 1, smp, mipLevel);
  uav[id].tex     = tex;
  uavUsage.durty |= (t.nonUniqId!=0);
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer* b, size_t offset) {
  auto&  device     = *lay.handler->dev.device;
  auto&  buf        = *reinterpret_cast<DxBuffer*>(b);
  auto&  prm        = lay.handler->prm[id];
  auto&  l          = lay.handler->lay[id];

  auto   descPtr    = val.cpu[HEAP_RES];
  auto   heapOffset = prm.heapOffset;

  if(lay.handler->isRuntimeSized()) {
    descPtr = heapDyn[HEAP_RES]!=nullptr ? heapDyn[HEAP_RES]->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE();
    heapOffset = runtimeArrays[id].heapOffset;
    runtimeArrays[id].data   = {b};
    runtimeArrays[id].offset = offset;
    }

  placeInHeap(device, prm.rgnType, descPtr, heapOffset, buf, offset, l.byteSize);

  uav[id].buf    = b;
  uavUsage.durty = true;
  }

void DxDescriptorArray::set(size_t id, const Sampler& smp) {
  auto& device = *lay.handler->dev.device;
  auto& prm    = lay.handler->prm[id];

  auto  smpPtr        = val.cpu[HEAP_SMP];
  auto  heapOffsetSmp = prm.heapOffset;

  if(lay.handler->isRuntimeSized()) {
    smpPtr = heapDyn[HEAP_SMP]!=nullptr ? heapDyn[HEAP_SMP]->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE();
    heapOffsetSmp = runtimeArrays[id].heapOffset;
    runtimeArrays[id].smp = smp;
    }

  placeInHeap(device, prm.rgnType, smpPtr, heapOffsetSmp, smp);
  }

void DxDescriptorArray::setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure* t) {
  auto& device = *lay.handler->dev.device;
  auto& tlas   = *reinterpret_cast<DxAccelerationStructure*>(t);
  auto& prm    = lay.handler->prm[id];

  auto  gpu        = val.cpu[HEAP_RES];
  auto  heapOffset = prm.heapOffset;
  if(lay.handler->isRuntimeSized()) {
    gpu        = heapDyn[HEAP_RES]!=nullptr ? heapDyn[HEAP_RES]->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE();
    heapOffset = runtimeArrays[id].heapOffset;
    runtimeArrays[id].data = {t};
    }

  placeInHeap(device, prm.rgnType, gpu, heapOffset, tlas);
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt, const Sampler& smp, uint32_t mipLevel) {
  auto& device = *lay.handler->dev.device;
  auto& prm    = lay.handler->prm[id];
  auto& l      = lay.handler->lay[id];

  if(l.runtimeSized) {
    constexpr uint32_t granularity = 1; //DxPipelineLay::MAX_BINDLESS;
    uint32_t rSz = ((cnt+granularity-1u) & (~(granularity-1u)));
    if(rSz!=runtimeArrays[id].size) {
      auto prev  = std::move(runtimeArrays[id].data);
      runtimeArrays[id].data.assign(tex, tex+cnt);
      try {
        reallocSet(id, rSz);
        runtimeArrays[id].size     = rSz;
        runtimeArrays[id].mipLevel = mipLevel;
        runtimeArrays[id].smp      = smp;
        }
      catch(...) {
        runtimeArrays[id].data = std::move(prev);
        throw;
        }
      reflushSet();
      return;
      }
    }

  uint32_t descSize = 0;
  uint32_t smpSize  = 0;
  if(cnt>1) {
    descSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    smpSize  = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

  D3D12_CPU_DESCRIPTOR_HANDLE descPtr       = val.cpu[HEAP_RES];
  D3D12_CPU_DESCRIPTOR_HANDLE smpPtr        = val.cpu[HEAP_SMP];
  UINT64                      heapOffset    = prm.heapOffset;
  UINT64                      heapOffsetSmp = prm.heapOffsetSmp;

  if(lay.handler->isRuntimeSized()) {
    descPtr       = heapDyn[0]!=nullptr ? heapDyn[0]->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE();
    smpPtr        = heapDyn[1]!=nullptr ? heapDyn[1]->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE();
    heapOffset    = runtimeArrays[id].heapOffset;
    heapOffsetSmp = runtimeArrays[id].heapOffsetSmp;
    }

  for(size_t i=0; i<cnt; ++i) {
    if(tex[i]==nullptr)
      continue;
    auto& t = *reinterpret_cast<DxTexture*>(tex[i]);
    placeInHeap(device, prm.rgnType, descPtr, heapOffset + i*descSize, t, smp.mapping, mipLevel);
    if(l.hasSampler())
      placeInHeap(device, prm.rgnType, smpPtr, heapOffsetSmp + i*smpSize, smp);
    }
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer** b, size_t cnt) {
  auto&    device = *lay.handler->dev.device;
  auto&    prm    = lay.handler->prm[id];
  auto&    l      = lay.handler->lay[id];

  if(l.runtimeSized) {
    constexpr uint32_t granularity = 1; //DxPipelineLay::MAX_BINDLESS;
    uint32_t rSz = ((cnt+granularity-1u) & (~(granularity-1u)));
    if(rSz!=runtimeArrays[id].size) {
      auto prev  = std::move(runtimeArrays[id].data);
      runtimeArrays[id].data.assign(b, b+cnt);
      try {
        reallocSet(id, rSz);
        runtimeArrays[id].size   = rSz;
        runtimeArrays[id].offset = 0;
        }
      catch(...) {
        runtimeArrays[id].data = std::move(prev);
        throw;
        }
      reflushSet();
      return;
      }
    }

  uint32_t descSize = 0;
  if(cnt>1)
    descSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  D3D12_CPU_DESCRIPTOR_HANDLE descPtr    = val.cpu[HEAP_RES];
  UINT64                      heapOffset = prm.heapOffset;

  if(lay.handler->isRuntimeSized()) {
    descPtr = heapDyn[0]!=nullptr ? heapDyn[0]->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE();
    heapOffset = runtimeArrays[id].heapOffset;
    }

  for(size_t i=0; i<cnt; ++i) {
    if(b[i]==nullptr)
      continue;
    auto&  buf    = *reinterpret_cast<DxBuffer*>(b[i]);
    placeInHeap(device, prm.rgnType, descPtr, heapOffset + i*descSize, buf, 0, lay.handler->lay[id].byteSize);
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

void DxDescriptorArray::bindRuntimeSized(ID3D12GraphicsCommandList6& enc, ID3D12DescriptorHeap** currentHeaps, bool isCompute) {
  // auto& device = *lay.handler->dev.device;
  auto& lx     = *lay.handler;

  if(currentHeaps[HEAP_RES]!=heapDyn[HEAP_RES] || currentHeaps[HEAP_SMP]!=heapDyn[HEAP_SMP]) {
    // TODO: single heap case
    currentHeaps[HEAP_RES] = heapDyn[HEAP_RES];
    currentHeaps[HEAP_SMP] = heapDyn[HEAP_SMP];

    const uint8_t cnt = (heapDyn[HEAP_SMP]==nullptr ? 1 : 2);
    enc.SetDescriptorHeaps(cnt, heapDyn);
    }

  D3D12_GPU_DESCRIPTOR_HANDLE descPtr  = heapDyn[HEAP_RES]!=nullptr ? heapDyn[HEAP_RES]->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE();
  D3D12_GPU_DESCRIPTOR_HANDLE smpPtr   = heapDyn[HEAP_SMP]!=nullptr ? heapDyn[HEAP_SMP]->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE();

  for(size_t i=0; i<lx.roots.size(); ++i) {
    auto&                       r    = lx.roots[i];
    D3D12_GPU_DESCRIPTOR_HANDLE desc = {};

    if(r.heap==HEAP_RES)
      desc = descPtr; else
      desc = smpPtr;

    if(r.binding==uint32_t(-1)) {
      desc.ptr += r.heapOffset;
      } else {
      UINT64 heapOffset    = runtimeArrays[r.binding].heapOffset;
      UINT64 heapOffsetSmp = runtimeArrays[r.binding].heapOffsetSmp;
      if(r.heap==HEAP_RES)
        desc.ptr += heapOffset; else
        desc.ptr += heapOffsetSmp;
      }

    if(isCompute)
      enc.SetComputeRootDescriptorTable (UINT(i), desc); else
      enc.SetGraphicsRootDescriptorTable(UINT(i), desc);
    }
  }

void DxDescriptorArray::reallocSet(size_t id, uint32_t newRuntimeSz) {
  auto&  device        = *lay.handler->dev.device;

  const uint32_t descSize      = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  const uint32_t smpSize       = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  const size_t   heapOffset    = lay.handler->heaps[0].numDesc;
  const size_t   heapOffsetSmp = lay.handler->heaps[1].numDesc;

  size_t lenOld[HEAP_MAX] = {heapOffset, heapOffsetSmp};
  size_t len   [HEAP_MAX] = {heapOffset, heapOffsetSmp};
  for(size_t i=0; i<runtimeArrays.size(); ++i) {
    auto& l       = lay.handler->lay[i];
    auto& prm     = lay.handler->prm[i];
    auto  size    = (i==id ? newRuntimeSz : runtimeArrays[i].size);
    auto  sizeOld = runtimeArrays[i].size;

    if(l.runtimeSized) {
      runtimeArrays[i].heapOffset    = len[HEAP_RES]*descSize;
      runtimeArrays[i].heapOffsetSmp = len[HEAP_SMP]*smpSize;
      } else {
      runtimeArrays[i].heapOffset    = prm.heapOffset;
      runtimeArrays[i].heapOffsetSmp = prm.heapOffsetSmp;
      }

    if(l.cls!=ShaderReflection::Sampler) {
      len   [HEAP_RES] += size;
      lenOld[HEAP_RES] += sizeOld;
      }

    if(l.hasSampler()) {
      len   [HEAP_SMP] += size;
      lenOld[HEAP_SMP] += sizeOld;
      }
    }

  ID3D12DescriptorHeap* heapDesc = nullptr;
  ID3D12DescriptorHeap* heapSmp  = nullptr;
  try {
    if((len[0]!=lenOld[0] || heapDyn[0]==nullptr) && len[0]>0) {
      D3D12_DESCRIPTOR_HEAP_DESC d = {};
      d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      d.NumDescriptors = UINT(len[0]);
      d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      dxAssert(device.CreateDescriptorHeap(&d, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&heapDesc)));
      }

    if((len[1]!=lenOld[1] || heapDyn[1]==nullptr) && len[1]>0) {
      D3D12_DESCRIPTOR_HEAP_DESC d = {};
      d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
      d.NumDescriptors = UINT(len[1]);
      d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      dxAssert(device.CreateDescriptorHeap(&d, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&heapSmp)));
      }
    }
  catch(...) {
    if(heapDesc!=nullptr)
      heapDesc->Release();
    if(heapSmp!=nullptr)
      heapSmp->Release();
    throw;
    }

  if(heapDesc!=nullptr || len[HEAP_RES]==0)
    std::swap(heapDyn[0], heapDesc);
  if(heapSmp !=nullptr || len[HEAP_SMP]==0)
    std::swap(heapDyn[1], heapSmp);

  if(heapDesc!=nullptr)
    heapDesc->Release();
  if(heapSmp!=nullptr)
    heapSmp->Release();
  }

void DxDescriptorArray::reflushSet() {
  auto& device = *lay.handler->dev.device;

  uint32_t descSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  uint32_t smpSize  = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  D3D12_CPU_DESCRIPTOR_HANDLE descPtr = heapDyn[0]!=nullptr ? heapDyn[0]->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE();
  D3D12_CPU_DESCRIPTOR_HANDLE smpPtr  = heapDyn[1]!=nullptr ? heapDyn[1]->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE();

  for(size_t id=0; id<lay.handler->lay.size(); ++id) {
    auto&  prm           = lay.handler->prm[id];
    auto&  l             = lay.handler->lay[id];

    auto&  arr           = runtimeArrays[id].data;
    auto&  smp           = runtimeArrays[id].smp;
    auto   mipLevel      = runtimeArrays[id].mipLevel;
    auto   offset        = runtimeArrays[id].offset;

    UINT64 heapOffset    = runtimeArrays[id].heapOffset;
    UINT64 heapOffsetSmp = runtimeArrays[id].heapOffsetSmp;

    switch (l.cls) {
      case ShaderReflection::Sampler: {
        placeInHeap(device, prm.rgnType, smpPtr, heapOffsetSmp, smp);
        break;
        }
      case ShaderReflection::Ubo:
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW: {
        for(size_t i=0; i<arr.size(); ++i) {
          auto* b = reinterpret_cast<DxBuffer*>(arr[i]);
          if(b==nullptr)
            continue;
          placeInHeap(device, prm.rgnType, descPtr, heapOffset + i*descSize, *b, offset, l.byteSize);
          }
        break;
        }
      case ShaderReflection::Tlas: {
        auto* t = reinterpret_cast<DxAccelerationStructure*>(arr[0]);
        if(t!=nullptr)
          placeInHeap(device, prm.rgnType, descPtr, heapOffset, *t);
        break;
        }
      case ShaderReflection::Texture:
      case ShaderReflection::Image:
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW: {
        for(size_t i=0; i<arr.size(); ++i) {
          auto* t = reinterpret_cast<DxTexture*>(arr[i]);
          if(t==nullptr)
            continue;
          placeInHeap(device, prm.rgnType, descPtr, heapOffset + i*descSize, *t, smp.mapping, mipLevel);
          if(l.hasSampler())
            placeInHeap(device, prm.rgnType, smpPtr, heapOffsetSmp + i*smpSize, smp);
          }
        break;
        }
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        break;
      }
    }
  }

void DxDescriptorArray::placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn,
                                    const D3D12_CPU_DESCRIPTOR_HANDLE& at, UINT64 heapOffset,
                                    DxTexture& t, const ComponentMapping& mapping, uint32_t mipLevel) {
  if(rgn==D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
    if(mipLevel==uint32_t(-1))
      mipLevel = 0;
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.Format             = t.format;
    if(t.sliceCnt>1) {
      desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE3D;
      desc.Texture3D.MipSlice = mipLevel;
      desc.Texture3D.WSize    = t.sliceCnt;
      } else {
      desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
      desc.Texture2D.MipSlice = mipLevel;
      }

    // auto  gpu = val.cpu[prm.heapId];
    // gpu.ptr += (prm.heapOffset + i*descSize);
    auto gpu = at;
    gpu.ptr += heapOffset;
    device.CreateUnorderedAccessView(t.impl.get(),nullptr,&desc,gpu);
    } else {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = compMapping(mapping);
    srvDesc.Format                  = t.format;
    if(t.sliceCnt>1) {
      srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE3D;
      srvDesc.Texture3D.MipLevels     = t.mips;
      //srvDesc.Texture3D.WSize         = t.sliceCnt;
      if(mipLevel!=uint32_t(-1)) {
        srvDesc.Texture3D.MostDetailedMip = mipLevel;
        srvDesc.Texture3D.MipLevels       = 1;
        }
      } else {
      srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels     = t.mips;
      if(mipLevel!=uint32_t(-1)) {
        srvDesc.Texture2D.MostDetailedMip = mipLevel;
        srvDesc.Texture2D.MipLevels       = 1;
        }
      }

    if(srvDesc.Format==DXGI_FORMAT_D16_UNORM)
      srvDesc.Format = DXGI_FORMAT_R16_UNORM;
    else if(srvDesc.Format==DXGI_FORMAT_D32_FLOAT)
      srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    else if(srvDesc.Format==DXGI_FORMAT_D24_UNORM_S8_UINT)
      srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

    // auto  gpu = val.cpu[prm.heapId];
    // gpu.ptr += (prm.heapOffset + i*descSize);
    auto gpu = at;
    gpu.ptr += heapOffset;
    device.CreateShaderResourceView(t.impl.get(), &srvDesc, gpu);
    }
  }

void DxDescriptorArray::placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn,
                                    const D3D12_CPU_DESCRIPTOR_HANDLE& at, UINT64 heapOffset, const Sampler& smp) {
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

  auto gpu = at;
  gpu.ptr += heapOffset;
  device.CreateSampler(&smpDesc,gpu);
  }

void DxDescriptorArray::placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn,
                                    const D3D12_CPU_DESCRIPTOR_HANDLE& at, UINT64 heapOffset, DxBuffer& buf,
                                    uint64_t bufOffset, uint64_t byteSize) {
  if(rgn==D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.Format              = DXGI_FORMAT_R32_TYPELESS;
    desc.ViewDimension       = D3D12_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = UINT(bufOffset/4);
    desc.Buffer.NumElements  = UINT((byteSize+3)/4); // UAV size is required to be 4-byte aligned.
    desc.Buffer.Flags        = D3D12_BUFFER_UAV_FLAG_RAW;
    if(desc.Buffer.NumElements==0)
      desc.Buffer.NumElements = UINT(buf.sizeInBytes-bufOffset)/4;

    auto gpu = at;
    gpu.ptr += heapOffset;
    device.CreateUnorderedAccessView(buf.impl.get(),nullptr,&desc,gpu);
    }
  else if(rgn==D3D12_DESCRIPTOR_RANGE_TYPE_SRV) {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format                  = DXGI_FORMAT_R32_TYPELESS;
    desc.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Buffer.FirstElement     = UINT(bufOffset/4);
    desc.Buffer.NumElements      = UINT((byteSize+3)/4); // SRV size is required to be 4-byte aligned.
    desc.Buffer.Flags            = D3D12_BUFFER_SRV_FLAG_RAW;
    if(desc.Buffer.NumElements==0)
      desc.Buffer.NumElements = UINT(buf.sizeInBytes-bufOffset)/4;

    auto gpu = at;
    gpu.ptr += heapOffset;
    device.CreateShaderResourceView(buf.impl.get(),&desc,gpu);
    }
  else {
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
    desc.BufferLocation = buf.impl->GetGPUVirtualAddress() + bufOffset;
    desc.SizeInBytes    = UINT(byteSize);
    if(desc.SizeInBytes==0)
      desc.SizeInBytes = UINT(buf.sizeInBytes-bufOffset);
    desc.SizeInBytes = ((desc.SizeInBytes+255)/256)*256; // CB size is required to be 256-byte aligned.

    auto gpu = at;
    gpu.ptr += heapOffset;
    device.CreateConstantBufferView(&desc, gpu);
    }
  }

void DxDescriptorArray::placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn, const D3D12_CPU_DESCRIPTOR_HANDLE& at,
                                    UINT64 heapOffset, DxAccelerationStructure& as) {
  D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
  desc.Format                  = DXGI_FORMAT_UNKNOWN;
  desc.ViewDimension           = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
  desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  desc.RaytracingAccelerationStructure.Location = as.impl.impl->GetGPUVirtualAddress();

  auto gpu = at;
  gpu.ptr += heapOffset;
  device.CreateShaderResourceView(nullptr,&desc,gpu);
  }

#endif
