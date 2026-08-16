#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxdescriptorarray.h"

#include "dxaccelerationstructure.h"
#include "dxbuffer.h"
#include "dxdevice.h"
#include "dxtexture.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

DxDescriptorArray::DxDescriptorArray(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel)
  : DxDescriptorArray(dev, tex, cnt, mipLevel, nullptr) {
  }

DxDescriptorArray::DxDescriptorArray(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler* sampler)
  : dev(dev), cnt(cnt) {
  //NOTE: no bindless storage image
  try {
    dPtrR = dev.descAlloc.allocRes(uint32_t(cnt));

    auto res = dev.descAlloc.handleCpu(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrR);
    for(size_t i=0; i<cnt; ++i) {
      DxPushDescriptor::write(dev, res, D3D12_CPU_DESCRIPTOR_HANDLE(), ShaderReflection::Image, tex[i], mipLevel, ComponentMapping(), Sampler::nearest());
      res.ptr += dev.descAlloc.resSize;
      }

    if(sampler!=nullptr) {
      dPtrS = dev.descAlloc.allocSmp(uint32_t(cnt));
      auto smp = dev.descAlloc.handleCpu(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, dPtrS);
      for(size_t i=0; i<cnt; ++i) {
        DxPushDescriptor::write(dev, D3D12_CPU_DESCRIPTOR_HANDLE(), smp, ShaderReflection::Sampler, nullptr, mipLevel, ComponentMapping(), *sampler);
        smp.ptr += dev.descAlloc.smpSize;
        }
      }
    }
  catch(...) {
    clear();
    throw;
    }
  }

DxDescriptorArray::DxDescriptorArray(DxDevice& dev, AbstractGraphicsApi::Buffer** buf, size_t cnt)
  : dev(dev), cnt(cnt) {
  try {
    dPtrRW = dev.descAlloc.allocRes(uint32_t(cnt));
    dPtrR  = dev.descAlloc.allocRes(uint32_t(cnt));

    auto resRW = dev.descAlloc.handleCpu(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrRW);
    auto resR  = dev.descAlloc.handleCpu(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrR );
    for(size_t i=0; i<cnt; ++i) {
      DxPushDescriptor::write(dev, resRW, D3D12_CPU_DESCRIPTOR_HANDLE(), ShaderReflection::SsboRW, buf[i], 0, ComponentMapping(), Sampler::nearest());
      DxPushDescriptor::write(dev, resR,  D3D12_CPU_DESCRIPTOR_HANDLE(), ShaderReflection::SsboR,  buf[i], 0, ComponentMapping(), Sampler::nearest());
      resRW.ptr += dev.descAlloc.resSize;
      resR.ptr  += dev.descAlloc.resSize;
      }
    }
  catch(...) {
    clear();
    throw;
    }
  }

DxDescriptorArray::~DxDescriptorArray() {
  clear();
  }

size_t DxDescriptorArray::size() const {
  return cnt;
  }

D3D12_GPU_DESCRIPTOR_HANDLE DxDescriptorArray::handleRW() const {
  return dev.descAlloc.handleGpu(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrRW);
  }

D3D12_GPU_DESCRIPTOR_HANDLE DxDescriptorArray::handleR() const {
  return dev.descAlloc.handleGpu(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrR);
  }

D3D12_GPU_DESCRIPTOR_HANDLE DxDescriptorArray::handleS() const {
  return dev.descAlloc.handleGpu(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, dPtrS);
  }

void DxDescriptorArray::clear() {
  dev.descAlloc.free(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrR,  uint32_t(cnt));
  dev.descAlloc.free(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dPtrRW, uint32_t(cnt));
  dev.descAlloc.free(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     dPtrS,  uint32_t(cnt));
  }

#endif

