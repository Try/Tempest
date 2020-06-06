#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxdescriptorarray.h"

#include <Tempest/UniformsLayout>

#include "dxbuffer.h"
#include "dxdevice.h"
#include "dxtexture.h"
#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

DxDescriptorArray::DxDescriptorArray(DxDevice& dev, const UniformsLayout& /*lay*/, DxUniformsLay& vlay)
  : dev(dev), layPtr(&vlay) {
  auto& device=*dev.device;

  D3D12_DESCRIPTOR_HEAP_DESC desc[DxUniformsLay::VisTypeCount*DxUniformsLay::MaxPrmPerStage] = {};
  size_t                     descCount = vlay.heaps.size();
  for(size_t i=0;i<descCount;++i) {
    auto& d          = desc[i];
    d.Type           = vlay.heaps[i].type;
    d.NumDescriptors = vlay.heaps[i].numDesc;
    d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }

  for(size_t i=0;i<descCount;++i) {
    dxAssert(device.CreateDescriptorHeap(&desc[i], uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&heap[i])));
    }
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture* tex, const Sampler2d& smp) {
  auto&      device = *dev.device;
  DxTexture& t      = *reinterpret_cast<DxTexture*>(tex);

  // Describe and create a SRV for the texture.
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; //TODO: R/RG textures
  srvDesc.Format                  = t.format;
  srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels     = t.mips;

  UINT filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  D3D12_SAMPLER_DESC smpDesc = {};
  smpDesc.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
  smpDesc.AddressU         = nativeFormat(smp.uClamp);
  smpDesc.AddressV         = nativeFormat(smp.vClamp);
  smpDesc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  smpDesc.MipLODBias       = 0;
  smpDesc.MaxAnisotropy    = 0;
  smpDesc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
  smpDesc.BorderColor[0]   = 0;
  smpDesc.BorderColor[1]   = 0;
  smpDesc.BorderColor[2]   = 0;
  smpDesc.BorderColor[3]   = 0;
  smpDesc.MinLOD           = 0.0f;
  smpDesc.MaxLOD           = FLOAT(t.mips);

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

  auto& prm = layPtr.handler->prm[id];
  auto  gpu = heap[prm.heapId]->GetCPUDescriptorHandleForHeapStart();
  gpu.ptr += prm.heapOffset;
  device.CreateShaderResourceView(t.impl.get(), &srvDesc, gpu);

  gpu = heap[prm.heapIdSmp]->GetCPUDescriptorHandleForHeapStart();
  gpu.ptr += prm.heapOffset;
  device.CreateSampler(&smpDesc,gpu);
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer* b, size_t offset, size_t /*size*/, size_t /*align*/) {
  assert(id==0);
  assert(offset==0);

  auto&      device = *dev.device;
  DxBuffer&  buf    = *reinterpret_cast<DxBuffer*>(b);
  // Describe and create a constant buffer view.
  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
  cbvDesc.BufferLocation = buf.impl->GetGPUVirtualAddress();
  cbvDesc.SizeInBytes    = (sizeof(buf.sizeInBytes) + 255) & ~255;    // CB size is required to be 256-byte aligned.

  auto& prm = layPtr.handler->prm[id];
  auto  gpu = heap[prm.heapId]->GetCPUDescriptorHandleForHeapStart();
  gpu.ptr += prm.heapOffset;
  device.CreateConstantBufferView(&cbvDesc, gpu);
  }

#endif
