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

DxDescriptorArray::DxDescriptorArray(DxDevice& dev, const UniformsLayout& lay, DxUniformsLay& vlay)
  : dev(dev), layPtr(&vlay) {
  auto& device=*dev.device;

  D3D12_DESCRIPTOR_HEAP_DESC desc[2] = {};
  for(auto& i:desc) {
    i.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    i.NumDescriptors = 0;
    i.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }

  for(size_t i=0; i<lay.size(); ++i){
    switch(lay[i].stage) {
      case UniformsLayout::Vertex:
        desc[0].NumDescriptors++;
        break;
      case UniformsLayout::Fragment:
        desc[1].NumDescriptors++;
        break;
      }
    }

  for(size_t i=0;i<2;++i) {
    if(desc[i].NumDescriptors==0)
      continue;
    dxAssert(device.CreateDescriptorHeap(&desc[i], uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&heap[i])));
    }
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture* tex) {
  auto&      device = *dev.device;
  DxTexture& t      = *reinterpret_cast<DxTexture*>(tex);

  // Describe and create a SRV for the texture.
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format                  = t.format;
  srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels     = t.mips;

  auto& prm = layPtr.handler->prm[id];
  auto  gpu = heap[prm.heapId]->GetCPUDescriptorHandleForHeapStart();
  gpu.ptr += prm.heapOffset;
  device.CreateShaderResourceView(t.impl.get(), &srvDesc, gpu);
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
