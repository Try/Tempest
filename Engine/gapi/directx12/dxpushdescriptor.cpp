#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxpushdescriptor.h"

#include "gapi/directx12/dxaccelerationstructure.h"
#include "gapi/directx12/dxdevice.h"
#include "gapi/directx12/dxtexture.h"
#include "gapi/directx12/dxbuffer.h"

using namespace Tempest;
using namespace Tempest::Detail;

static std::pair<uint32_t, uint32_t> numResources(const ShaderReflection::LayoutDesc& lay) {
  std::pair<uint32_t, uint32_t> ret;
  for(size_t i=0; i<MaxBindings; ++i) {
    if(((1u << i) & lay.array)!=0)
      continue;
    switch(lay.bindings[i]) {
      case ShaderReflection::Sampler:
        ret.second++;
        break;
      case ShaderReflection::Texture:
        ret.first++;
        ret.second++;
        break;
      case ShaderReflection::Ubo:
      case ShaderReflection::Image:
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW:
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW:
      case ShaderReflection::Tlas:
        ret.first++;
        break;
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        break;
      }
    }
  return ret;
  }

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


template<enum D3D12_DESCRIPTOR_HEAP_TYPE HeapType>
DxPushDescriptor::Pool<HeapType>::Pool(DxDevice& dev, uint32_t size) {
  dPtr  = dev.dalloc->alloc(HeapType, size);
  alloc = 0;
  }


DxPushDescriptor::DxPushDescriptor(DxDevice& dev)
  : dev(dev) {
  }

DxPushDescriptor::~DxPushDescriptor() {
  reset();
  }

void DxPushDescriptor::reset() {
  resPool.reserve(resPool.size());
  smpPool.reserve(smpPool.size());
  for(auto& i:resPool) {
    dev.dalloc->free(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, i.dPtr, RES_ALLOC_SZ);
    }
  for(auto& i:smpPool) {
    dev.dalloc->free(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, i.dPtr, SMP_ALLOC_SZ);
    }
  resPool.clear();
  smpPool.clear();
  }

template<D3D12_DESCRIPTOR_HEAP_TYPE T>
uint32_t DxPushDescriptor::alloc(std::vector<Pool<T>>& pool, const uint32_t sz, const uint32_t step) {
  if(pool.empty())
    pool.emplace_back(dev, step);

  if((step-pool.back().alloc) < sz)
    pool.emplace_back(dev, step);

  auto& px = pool.back();
  const uint32_t ptr = px.dPtr + px.alloc;
  px.alloc += sz;
  return ptr;
  }

std::pair<uint32_t, uint32_t> DxPushDescriptor::alloc(uint32_t numRes, uint32_t numSmp) {
  auto rptr = alloc<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(resPool, numRes, RES_ALLOC_SZ);
  auto sptr = alloc<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>    (smpPool, numSmp, SMP_ALLOC_SZ);
  return std::make_pair(rptr, sptr);
  }

uint32_t DxPushDescriptor::alloc(uint32_t numRes) {
  return alloc<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(resPool, numRes, RES_ALLOC_SZ);
  }

DxPushDescriptor::DescSet DxPushDescriptor::push(const PushBlock &pb, const LayoutDesc& lay, const Bindings& binding) {
  const auto sz  = numResources(lay);
  const auto ptr = alloc(sz.first, sz.second);

  auto res = dev.dalloc->res;
  res.ptr += ptr.first*dev.dalloc->resSize;

  auto smp = dev.dalloc->smp;
  smp.ptr += ptr.second*dev.dalloc->smpSize;

  for(size_t i=0; i<MaxBindings; ++i) {
    if(((1u << i) & lay.active)==0)
      continue;
    if(((1u << i) & lay.array)!=0)
      continue;

    write(dev, res, smp, lay.bindings[i], binding.data[i], binding.offset[i], binding.map[i], binding.smp[i]);

    if(lay.bindings[i]!=ShaderReflection::Sampler)
      res.ptr += dev.dalloc->resSize;

    if(lay.bindings[i]==ShaderReflection::Sampler || lay.bindings[i]==ShaderReflection::Texture)
      smp.ptr += dev.dalloc->smpSize;
    }

  DescSet ret = {};
  if(sz.first!=0) {
    ret.res = dev.dalloc->gres;
    ret.res.ptr += ptr.first*dev.dalloc->resSize;
    }
  if(sz.second!=0) {
    ret.smp = dev.dalloc->gsmp;
    ret.smp.ptr += ptr.second*dev.dalloc->smpSize;
    }

  return ret;
  }

void DxPushDescriptor::setRootTable(ID3D12GraphicsCommandList& enc, const DescSet& set, const LayoutDesc& lay, const Bindings& binding, const PipelineStage st) {
  uint32_t id = 0;
  if(set.res.ptr!=0) {
    setRootDescriptorTable(enc, st, id, set.res);
    ++id;
    }
  if(set.smp.ptr!=0) {
    setRootDescriptorTable(enc, st, id, set.smp);
    ++id;
    }

  for(size_t i=0; i<MaxBindings; ++i) {
    if(((1u << i) & lay.array)==0)
      continue;

    auto* a = reinterpret_cast<const DxDescriptorArray*>(binding.data[i]);
    if(lay.bindings[i]==ShaderReflection::SsboRW)
      setRootDescriptorTable(enc, st, id, a->handleRW()); else
      setRootDescriptorTable(enc, st, id, a->handleR());
    ++id;

    if(lay.bindings[i]==ShaderReflection::Texture) {
      setRootDescriptorTable(enc, st, id, a->handleS());
      ++id;
      }
    }
  }

void DxPushDescriptor::setRootDescriptorTable(ID3D12GraphicsCommandList& enc, const PipelineStage st,
                                              UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
  if(st==S_Compute)
    enc.SetComputeRootDescriptorTable (RootParameterIndex, BaseDescriptor); else
    enc.SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);
  }

void DxPushDescriptor::write(DxDevice& dev, D3D12_CPU_DESCRIPTOR_HANDLE res, D3D12_CPU_DESCRIPTOR_HANDLE s,
                             ShaderReflection::Class cls, AbstractGraphicsApi::NoCopy* data, uint32_t offset,
                             const ComponentMapping& mapping, const Sampler& smp) {
  auto& device = *dev.device;

  switch(cls) {
    case ShaderReflection::Sampler:
      write(dev, s, smp);
      break;
    case ShaderReflection::Image:
    case ShaderReflection::ImgR:
    case ShaderReflection::Texture: {
      auto* tex = reinterpret_cast<DxTexture*>(data);
      if(tex==nullptr)
        return;
      uint32_t mipLevel  = offset;
      bool     is3DImage = tex->is3D; // TODO: cast 3d to 2d, based on dest descriptor

      D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
      desc.Shader4ComponentMapping = compMapping(mapping);
      desc.Format                  = nativeSrvFormat(tex->format);
      if(is3DImage) {
        desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE3D;
        desc.Texture3D.MipLevels     = tex->mipCnt;
        //desc.Texture3D.WSize         = t.sliceCnt;
        if(mipLevel!=uint32_t(-1)) {
          desc.Texture3D.MostDetailedMip = mipLevel;
          desc.Texture3D.MipLevels       = 1;
          }
        } else {
        desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels     = tex->mipCnt;
        if(mipLevel!=uint32_t(-1)) {
          desc.Texture2D.MostDetailedMip = mipLevel;
          desc.Texture2D.MipLevels       = 1;
          }
        }

      device.CreateShaderResourceView(tex->impl.get(), &desc, res);
      if(cls==ShaderReflection::Texture) {
        auto sx = smp;
        if(!tex->isFilterable) {
          /*
           * https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_format_support1
           * If the device supports the format as a resource (1D, 2D, 3D, or cube map)
           * but doesn't support this option, the resource can still use the Sample method
           * but must use only the point filtering sampler state to perform the sample.
           */
          sx.setFiltration(Filter::Nearest);
          sx.anisotropic = false;
          }
        write(dev, s, sx);
        }
      break;
      }
    case ShaderReflection::ImgRW: {
      auto* tex = reinterpret_cast<DxTexture*>(data);
      if(tex==nullptr)
        return;
      uint32_t mipLevel  = offset;
      bool     is3DImage = tex->is3D; // TODO: cast 3d to 2d, based on dest descriptor

      if(mipLevel==uint32_t(-1))
        mipLevel = 0;
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
      desc.Format             = tex->format;
      if(is3DImage) {
        desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE3D;
        desc.Texture3D.MipSlice = mipLevel;
        desc.Texture3D.WSize    = tex->sliceCnt;
        } else {
        desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = mipLevel;
        }

      device.CreateUnorderedAccessView(tex->impl.get(), nullptr, &desc, res);
      break;
      }
    case ShaderReflection::Ubo: {
      auto* buf  = reinterpret_cast<DxBuffer*>(data);
      UINT  size = buf->appSize;

      D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
      desc.BufferLocation = buf->impl->GetGPUVirtualAddress() + offset;
      desc.SizeInBytes    = UINT(size);
      if(desc.SizeInBytes==0)
        desc.SizeInBytes = UINT(buf->sizeInBytes - offset);
      desc.SizeInBytes = ((desc.SizeInBytes+255)/256)*256; // CB size is required to be 256-byte aligned.

      device.CreateConstantBufferView(&desc, res);
      break;
      }
    case ShaderReflection::SsboR: {
      auto* buf  = reinterpret_cast<DxBuffer*>(data);
      UINT  size = buf!=nullptr ? buf->appSize : 0;

      D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
      desc.Format                  = DXGI_FORMAT_R32_TYPELESS;
      desc.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER;
      desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      desc.Buffer.FirstElement     = UINT(offset/4);
      desc.Buffer.NumElements      = UINT((size+3)/4); // SRV size is required to be 4-byte aligned.
      desc.Buffer.Flags            = D3D12_BUFFER_SRV_FLAG_RAW;
      if(desc.Buffer.NumElements==0 && buf!=nullptr)
        desc.Buffer.NumElements = UINT(buf->appSize-offset+3)/4;

      if(buf!=nullptr)
        device.CreateShaderResourceView(buf->impl.get(), &desc, res); else
        device.CreateShaderResourceView(nullptr, &desc, res);
      break;
      }
    case ShaderReflection::SsboRW: {
      auto* buf  = reinterpret_cast<DxBuffer*>(data);
      UINT  size = buf!=nullptr ? buf->appSize : 0;

      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
      desc.Format              = DXGI_FORMAT_R32_TYPELESS;
      desc.ViewDimension       = D3D12_UAV_DIMENSION_BUFFER;
      desc.Buffer.FirstElement = UINT(offset/4);
      desc.Buffer.NumElements  = UINT((size+3)/4); // UAV size is required to be 4-byte aligned.
      desc.Buffer.Flags        = D3D12_BUFFER_UAV_FLAG_RAW;
      if(desc.Buffer.NumElements==0 && buf!=nullptr)
        desc.Buffer.NumElements = UINT(buf->appSize-offset+3)/4;

      if(buf!=nullptr)
        device.CreateUnorderedAccessView(buf->impl.get(), nullptr, &desc, res); else
        device.CreateUnorderedAccessView(nullptr, nullptr, &desc, res);
      break;
      }
    case ShaderReflection::Tlas: {
      auto* tlas = reinterpret_cast<DxAccelerationStructure*>(data);

      D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
      desc.Format                  = DXGI_FORMAT_UNKNOWN;
      desc.ViewDimension           = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
      desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      desc.RaytracingAccelerationStructure.Location = tlas->impl.impl->GetGPUVirtualAddress();

      device.CreateShaderResourceView(nullptr, &desc, res);
      break;
      }
    case ShaderReflection::Push:
    case ShaderReflection::Count:
      break;
    }
  }

void DxPushDescriptor::write(DxDevice& dev, D3D12_CPU_DESCRIPTOR_HANDLE h, const Sampler& smp) {
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

  dev.device->CreateSampler(&smpDesc, h);
  }

#endif
