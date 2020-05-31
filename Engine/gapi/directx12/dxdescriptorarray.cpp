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

DxDescriptorArray::DxDescriptorArray(DxDevice& dev, const UniformsLayout& lay, DxUniformsLay& /*vlay*/)
  : dev(dev) {
  // Dscribe and create a constant buffer view (CBV) descriptor heap.
  // Flags indicate that this descriptor heap can be bound to the pipeline
  // and that descriptors contained in it can be referenced by a root table.
  auto& device=*dev.device;

  D3D12_DESCRIPTOR_HEAP_DESC descCb = {};
  descCb.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  descCb.NumDescriptors = 0;
  descCb.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  D3D12_DESCRIPTOR_HEAP_DESC descRtv = {};
  descRtv.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  descRtv.NumDescriptors = 0;
  descRtv.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  for(size_t i=0;i<lay.size();++i){
    auto cls = lay[i].cls;
    switch(cls) {
      case UniformsLayout::Ubo:     descCb .NumDescriptors++; break;
      case UniformsLayout::UboDyn:  descCb .NumDescriptors++; break;
      case UniformsLayout::Texture: descRtv.NumDescriptors++; break;
      }
    }

  if(descCb.NumDescriptors>0)
    dxAssert(device.CreateDescriptorHeap(&descCb,  uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&heapCb)));
  if(descRtv.NumDescriptors>0)
    dxAssert(device.CreateDescriptorHeap(&descRtv, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&heapSrv)));
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture* tex) {
  assert(id==0);

  auto&      device = *dev.device;
  DxTexture& t      = *reinterpret_cast<DxTexture*>(tex);

  // Describe and create a SRV for the texture.
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format                  = t.format;
  srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels     = t.mips;

  //m_rtvDescriptorSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  device.CreateShaderResourceView(t.impl.get(), &srvDesc, heapSrv->GetCPUDescriptorHandleForHeapStart());
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
  device.CreateConstantBufferView(&cbvDesc, heapCb->GetCPUDescriptorHandleForHeapStart());
  }

#endif
