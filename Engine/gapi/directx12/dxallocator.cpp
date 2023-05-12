#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxallocator.h"

#include "dxdevice.h"
#include "dxbuffer.h"
#include "dxtexture.h"
#include "guid.h"

#include "gapi/graphicsmemutils.h"

#include <Tempest/Pixmap>
#include <iostream>

using namespace Tempest;
using namespace Tempest::Detail;

static UINT64 toAlignment(BufferHeap heap) {
  // return D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
  return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
  }

DxAllocator::Provider::~Provider() {
  if(last!=nullptr)
    last->Release();
  }

DxAllocator::Provider::DeviceMemory DxAllocator::Provider::alloc(size_t size, uint32_t typeId) {
  if(last!=nullptr) {
    if(lastTypeId==typeId && lastSize==size){
      auto ret = last;
      last = nullptr;
      return ret;
      }
    last->Release();
    last = nullptr;
    }

  static const D3D12_HEAP_FLAGS D3D12_HEAP_FLAG_CREATE_NOT_ZEROED = D3D12_HEAP_FLAGS(0x1000);
  const auto bufferHeap = BufferHeap(typeId);

  D3D12_HEAP_DESC heapDesc = {};
  heapDesc.SizeInBytes                     = size;
  heapDesc.Properties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapDesc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapDesc.Properties.CreationNodeMask     = 1;
  heapDesc.Properties.VisibleNodeMask      = 1;
  heapDesc.Alignment                       = toAlignment(bufferHeap);

  heapDesc.Flags                           = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS | D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;

  if(bufferHeap==BufferHeap::Device) {
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapDesc.Flags          |= D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
    }
  else if(bufferHeap==BufferHeap::Upload) {
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    }
  else if(bufferHeap==BufferHeap::Readback) {
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_READBACK;
    }

  ID3D12Heap* ret = nullptr;
  HRESULT hr = device->device->CreateHeap(&heapDesc, ::uuid<ID3D12Heap>(), reinterpret_cast<void**>(&ret));
  if(SUCCEEDED(hr))
    return ret;
  return nullptr;
  }

void DxAllocator::Provider::free(DeviceMemory m, size_t size, uint32_t typeId) {
  if(last!=nullptr)
    last->Release();
  last       = m;
  lastSize   = size;
  lastTypeId = typeId;
  }

DxAllocator::DxAllocator() {
  auto& dev      = buferHeap[uint8_t(BufferHeap::Device)];
  auto& upload   = buferHeap[uint8_t(BufferHeap::Upload)];
  auto& readback = buferHeap[uint8_t(BufferHeap::Readback)];

  dev.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  dev.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  dev.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  dev.CreationNodeMask     = 1;
  dev.VisibleNodeMask      = 1;

  upload = dev;
  upload.Type              = D3D12_HEAP_TYPE_UPLOAD;

  readback = dev;
  readback.Type            = D3D12_HEAP_TYPE_READBACK;
  }

void DxAllocator::setDevice(DxDevice& d) {
  owner           = &d;
  device          = d.device.get();
  provider.device = owner;
  }

DxBuffer DxAllocator::alloc(const void* mem, size_t count, size_t size, size_t alignedSz,
                            MemUsage usage, BufferHeap bufFlg) {
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

  if(MemUsage::UniformBuffer==(usage&MemUsage::UniformBuffer) && resDesc.Width%256!=0) {
    // CB size is required to be 256-byte aligned.
    resDesc.Width += 256-resDesc.Width%256;
    }

  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
  if(bufFlg==BufferHeap::Upload) {
    state = D3D12_RESOURCE_STATE_GENERIC_READ;
    }
  else if(bufFlg==BufferHeap::Readback) {
    state = D3D12_RESOURCE_STATE_COPY_DEST;
    }

  if(MemUsage::StorageBuffer==(usage&MemUsage::StorageBuffer)) {
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    //state         |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }
  if(MemUsage::AsStorage==(usage&MemUsage::AsStorage)) {
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    state         |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }
  if(MemUsage::ScratchBuffer==(usage&MemUsage::ScratchBuffer)) {
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    state          = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

  DxBuffer ret(owner,UINT(resDesc.Width));
  ret.page = allocator.alloc(count*alignedSz, toAlignment(bufFlg),
                             uint32_t(bufFlg), uint32_t(bufFlg), (bufFlg!=BufferHeap::Device));
  if(!ret.page.page)
    throw std::system_error(Tempest::GraphicsErrc::OutOfVideoMemory);

  if(!commit(ret.page.page->memory,ret.page.page->mmapSync,ret.impl.get(),
             resDesc, state, ret.page.offset,
             mem,count,size,alignedSz)) {
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }

  ret.nonUniqId = (MemUsage::StorageBuffer==(usage&MemUsage::StorageBuffer)) ? NonUniqResId::I_Ssbo : NonUniqResId::I_Buf;
  return ret;
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
  if(mip>1 &&
     format!=DXGI_FORMAT_BC1_UNORM && format!=DXGI_FORMAT_BC2_UNORM && format!=DXGI_FORMAT_BC3_UNORM) {
    // for mip-maps generator
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }

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
  return DxTexture(std::move(ret),resDesc.Format,NonUniqResId::I_None,resDesc.MipLevels,1);
  }

DxTexture DxAllocator::alloc(const uint32_t w, const uint32_t h, const uint32_t d, const uint32_t mip, TextureFormat frm, bool imageStore) {
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
  D3D12_CLEAR_VALUE     clr   = {};

  D3D12_RESOURCE_DESC resDesc = {};
  resDesc.MipLevels          = mip;
  resDesc.Format             = Detail::nativeFormat(frm);
  resDesc.Width              = w;
  resDesc.Height             = h;
  resDesc.DepthOrArraySize   = d;
  if(isDepthFormat(frm)) {
    state                    = D3D12_RESOURCE_STATE_DEPTH_READ;
    resDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    clr.DepthStencil.Depth   = 1.f;
    clr.DepthStencil.Stencil = 0;
    } else {
    state                    = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    resDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    clr.Color[0] = 0.f;
    clr.Color[1] = 0.f;
    clr.Color[2] = 0.f;
    clr.Color[3] = 0.f;
    }
  if(imageStore) {
    state          = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
  resDesc.SampleDesc.Count   = 1;
  resDesc.SampleDesc.Quality = 0;
  resDesc.Dimension          = d<=1 ? D3D12_RESOURCE_DIMENSION_TEXTURE2D : D3D12_RESOURCE_DIMENSION_TEXTURE3D;

  D3D12_HEAP_PROPERTIES heapProp={};
  heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProp.CreationNodeMask     = 1;
  heapProp.VisibleNodeMask      = 1;

  clr.Format = resDesc.Format;

  ComPtr<ID3D12Resource> ret;
  dxAssert(device->CreateCommittedResource(
             &heapProp,
             D3D12_HEAP_FLAG_NONE,
             &resDesc,
             state,
             &clr,
             uuid<ID3D12Resource>(),
             reinterpret_cast<void**>(&ret)
             ));
  auto nonUniqId = (imageStore) ? NonUniqResId::I_Img : NonUniqResId::I_None;
  return DxTexture(std::move(ret),resDesc.Format,nonUniqId,resDesc.MipLevels,UINT(d));
  }

void DxAllocator::free(Allocation& page) {
  if(page.page!=nullptr)
    allocator.free(page);
  }

bool DxAllocator::commit(ID3D12Heap* heap, std::mutex& mmapSync,
                         ID3D12Resource*& dest, const D3D12_RESOURCE_DESC& resDesc, D3D12_RESOURCE_STATES state,
                         size_t offset, const void* mem, size_t count, size_t size, size_t alignedSz) {
  std::lock_guard<std::mutex> g(mmapSync);
  auto err = device->CreatePlacedResource(heap,
                                          offset,
                                          &resDesc,
                                          state,
                                          nullptr,
                                          uuid<ID3D12Resource>(),
                                          reinterpret_cast<void**>(&dest));
  if(FAILED(err))
    return false;
  if(mem!=nullptr) {
    D3D12_RANGE rgn = {0,resDesc.Width};
    void*       mapped=nullptr;
    if(FAILED(dest->Map(0,nullptr,&mapped)))
      return false;
    copyUpsample(mem,mapped,count,size,alignedSz);
    dest->Unmap(0,&rgn);
    }
  return true;
  }

#endif
