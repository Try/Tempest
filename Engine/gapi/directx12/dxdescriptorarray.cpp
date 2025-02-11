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

DxDescriptorArray2::DxDescriptorArray2(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel)
  : DxDescriptorArray2(dev, tex, cnt, mipLevel, nullptr) {
  }

DxDescriptorArray2::DxDescriptorArray2(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler* sampler)
  : dev(dev), cnt(cnt) {
  //NOTE: no bindless storage image
  try {
    auto sx = sampler!=nullptr ? *sampler : Sampler::nearest();
    dPtrR = dev.dalloc->alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uint32_t(cnt));

    for(size_t i=0; i<cnt; ++i) {
      auto res = dev.dalloc->res;
      res.ptr += (dPtrR + i)*dev.dalloc->resSize;
      DxPushDescriptor::write(dev, res, D3D12_CPU_DESCRIPTOR_HANDLE(), ShaderReflection::Image, tex[i], mipLevel, sx);
      }

    if(sampler!=nullptr) {
      dPtrS = dev.dalloc->alloc(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, uint32_t(cnt));
      for(size_t i=0; i<cnt; ++i) {
        auto smp = dev.dalloc->smp;
        smp.ptr += (dPtrS + i)*dev.dalloc->smpSize;
        DxPushDescriptor::write(dev, D3D12_CPU_DESCRIPTOR_HANDLE(), smp, ShaderReflection::Sampler, nullptr, mipLevel, sx);
        }
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

D3D12_GPU_DESCRIPTOR_HANDLE DxDescriptorArray2::handleS() const {
  auto res = dev.dalloc->gsmp;
  res.ptr += dPtrS*dev.dalloc->smpSize;
  return res;
  }

void DxDescriptorArray2::clear() {
  dev.dalloc->free(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrR,  uint32_t(cnt));
  dev.dalloc->free(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrRW, uint32_t(cnt));
  dev.dalloc->free(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     dPtrS,  uint32_t(cnt));
  }

#endif

