#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxallocator.h"

#include "dxdevice.h"
#include "dxbuffer.h"
#include "dxtexture.h"
#include "guid.h"

#include "gapi/graphicsmemutils.h"

#include <Tempest/Pixmap>

using namespace Tempest;
using namespace Tempest::Detail;

DxAllocator::DxAllocator() {
  }

void DxAllocator::setDevice(DxDevice& d) {
  device = d.device.get();
  }

DxBuffer DxAllocator::alloc(const void* mem, size_t count, size_t size, size_t alignedSz,
                            MemUsage /*usage*/, BufferHeap bufFlg) {
  ComPtr<ID3D12Resource> ret;

  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

  D3D12_RESOURCE_DESC resDesc={};
  resDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Alignment          = 0;
  resDesc.Width              = count*alignedSz;
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

  if(bufFlg==BufferHeap::Upload) {
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
    state         = D3D12_RESOURCE_STATE_GENERIC_READ;
    }
  if(bufFlg==BufferHeap::Readback) {
    heapProp.Type = D3D12_HEAP_TYPE_READBACK;
    state         = D3D12_RESOURCE_STATE_COPY_DEST;
    }

  if(resDesc.Width==0 || resDesc.Height==0)
    Log::d("");

  dxAssert(device->CreateCommittedResource(
             &heapProp,
             D3D12_HEAP_FLAG_NONE,
             &resDesc,
             state,
             nullptr,
             uuid<ID3D12Resource>(),
             reinterpret_cast<void**>(&ret)
             ));

  if(mem!=nullptr) {
    D3D12_RANGE rgn = {0,resDesc.Width};
    void*       mapped=nullptr;
    dxAssert(ret->Map(0,&rgn,&mapped));
    copyUpsample(mem,mapped,count,size,alignedSz);
    ret->Unmap(0,&rgn);
    }

  return DxBuffer(std::move(ret),UINT(resDesc.Width));
  }

DxTexture DxAllocator::alloc(const Pixmap& pm, uint32_t mip, DXGI_FORMAT format) {
  ComPtr<ID3D12Resource> ret;

  D3D12_RESOURCE_DESC resDesc = {};
  resDesc.MipLevels          = mip;
  resDesc.Format             = format;
  resDesc.Width              = pm.w();
  resDesc.Height             = pm.h();
  resDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;
  resDesc.DepthOrArraySize   = 1;
  resDesc.SampleDesc.Count   = 1;
  resDesc.SampleDesc.Quality = 0;
  resDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

  D3D12_HEAP_PROPERTIES heapProp={};
  heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProp.CreationNodeMask     = 1;
  heapProp.VisibleNodeMask      = 1;

  dxAssert(device->CreateCommittedResource(
             &heapProp,
             D3D12_HEAP_FLAG_NONE,
             &resDesc,
             D3D12_RESOURCE_STATE_COPY_DEST,
             nullptr,
             uuid<ID3D12Resource>(),
             reinterpret_cast<void**>(&ret)
             ));
  return DxTexture(std::move(ret),resDesc.Format,resDesc.MipLevels);
  }

DxTexture DxAllocator::alloc(const uint32_t w, const uint32_t h, const uint32_t mip, TextureFormat frm) {
  ComPtr<ID3D12Resource> ret;

  D3D12_RESOURCE_DESC resDesc = {};
  resDesc.MipLevels          = mip;
  resDesc.Format             = Detail::nativeFormat(frm);
  resDesc.Width              = w;
  resDesc.Height             = h;
  if(isDepthFormat(frm))
    resDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; else
    resDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  resDesc.DepthOrArraySize   = 1;
  resDesc.SampleDesc.Count   = 1;
  resDesc.SampleDesc.Quality = 0;
  resDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

  D3D12_HEAP_PROPERTIES heapProp={};
  heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProp.CreationNodeMask     = 1;
  heapProp.VisibleNodeMask      = 1;

  dxAssert(device->CreateCommittedResource(
             &heapProp,
             D3D12_HEAP_FLAG_NONE,
             &resDesc,
             D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
             nullptr,
             uuid<ID3D12Resource>(),
             reinterpret_cast<void**>(&ret)
             ));
  return DxTexture(std::move(ret),resDesc.Format,resDesc.MipLevels);
  }

#endif
