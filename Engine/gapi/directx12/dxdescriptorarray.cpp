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


DxDescriptorArray2::DxDescriptorArray2(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel)
  : DxDescriptorArray2(dev, tex, cnt, mipLevel, Sampler::nearest()) {
  }

DxDescriptorArray2::DxDescriptorArray2(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler& smp)
  : dev(dev), cnt(cnt) {
  //NOTE: no bindless storage image
  try {
    dPtrR = dev.dalloc->alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uint32_t(cnt));

    for(size_t i=0; i<cnt; ++i) {
      auto res = dev.dalloc->res;
      res.ptr += (dPtrR + i)*dev.dalloc->resSize;
      DxPushDescriptor::write(dev, res, D3D12_CPU_DESCRIPTOR_HANDLE(), ShaderReflection::Image, tex[i], mipLevel, smp);
      }
    }
  catch(...) {
    clear();
    throw;
    }
  }

DxDescriptorArray2::DxDescriptorArray2(DxDevice& dev, AbstractGraphicsApi::Buffer** buf, size_t cnt)
  : dev(dev), cnt(cnt) {
  try {
    dPtrRW = dev.dalloc->alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uint32_t(cnt));
    dPtrR  = dev.dalloc->alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uint32_t(cnt));

    for(size_t i=0; i<cnt; ++i) {
      auto res = dev.dalloc->res;
      res.ptr += (dPtrRW + i)*dev.dalloc->resSize;
      DxPushDescriptor::write(dev, res, D3D12_CPU_DESCRIPTOR_HANDLE(), ShaderReflection::SsboRW, buf[i], 0, Sampler::nearest());

      res = dev.dalloc->res;
      res.ptr += (dPtrR  + i)*dev.dalloc->resSize;
      DxPushDescriptor::write(dev, res, D3D12_CPU_DESCRIPTOR_HANDLE(), ShaderReflection::SsboR, buf[i], 0, Sampler::nearest());
      }
    }
  catch(...) {
    clear();
    throw;
    }
  }

DxDescriptorArray2::~DxDescriptorArray2() {
  clear();
  }

size_t DxDescriptorArray2::size() const {
  return cnt;
  }

D3D12_GPU_DESCRIPTOR_HANDLE DxDescriptorArray2::handleRW() const {
  auto res = dev.dalloc->gres;
  res.ptr += dPtrRW*dev.dalloc->resSize;
  return res;
  }

D3D12_GPU_DESCRIPTOR_HANDLE DxDescriptorArray2::handleR() const {
  auto res = dev.dalloc->gres;
  res.ptr += dPtrR*dev.dalloc->resSize;
  return res;
  }

void DxDescriptorArray2::clear() {
  dev.dalloc->free(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrR,  uint32_t(cnt));
  dev.dalloc->free(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrRW, uint32_t(cnt));
  }

#endif

