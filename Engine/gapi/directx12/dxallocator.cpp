#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxallocator.h"

#include "dxdevice.h"
#include "dxbuffer.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxAllocator::DxAllocator() {
  }

void DxAllocator::setDevice(DxDevice& d) {
  device = d.device.get();
  }

DxBuffer DxAllocator::alloc(const void* mem, size_t size, MemUsage usage, BufferFlags bufFlg) {
  ComPtr<ID3D12Resource> ret;

  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

  D3D12_RESOURCE_DESC resDesc={};
  resDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Alignment          = 0;
  resDesc.Width              = size;
  resDesc.Height             = 1;
  resDesc.DepthOrArraySize   = 1;
  resDesc.MipLevels          = 1;
  resDesc.Format             = DXGI_FORMAT_UNKNOWN;
  resDesc.SampleDesc.Count   = 1;
  resDesc.SampleDesc.Quality = 0;
  resDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

  D3D12_HEAP_PROPERTIES heapProp={};
  heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProp.CreationNodeMask     = 1;
  heapProp.VisibleNodeMask      = 1;

  if(bufFlg==BufferFlags::Dynamic || bufFlg==BufferFlags::Staging) {
    // TODO: it's better to align api-abstraction to directx model
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
    state         = D3D12_RESOURCE_STATE_GENERIC_READ;
    }

  dxAssert(device->CreateCommittedResource(
             &heapProp,
             D3D12_HEAP_FLAG_NONE,
             &resDesc,
             state,
             nullptr,
             uuid<ID3D12Resource>(),
             reinterpret_cast<void**>(&ret)
             ));

  D3D12_RANGE rgn = {0,size};
  void*       mapped=nullptr;
  dxAssert(ret->Map(0,&rgn,&mapped));
  std::memcpy(mapped,mem,size);
  ret->Unmap(0,&rgn);

  return DxBuffer(std::move(ret));
  }

#endif
