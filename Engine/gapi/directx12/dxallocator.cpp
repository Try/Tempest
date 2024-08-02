#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxallocator.h"

#include "dxdevice.h"
#include "dxbuffer.h"
#include "dxtexture.h"
#include "guid.h"

#include <Tempest/Pixmap>

using namespace Tempest;
using namespace Tempest::Detail;

static UINT64 toAlignment(BufferHeap heap) {
  // return D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
  return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
  }

static NonUniqResId nextId(){
  static std::atomic_uint32_t i = {};
  uint32_t ix = i.fetch_add(1) % 32;
  uint32_t b = 1u << ix;
  return NonUniqResId(b);
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
    // heapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapDesc.Properties.Type                 = D3D12_HEAP_TYPE_CUSTOM;
    heapDesc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
    heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    }
  else if(bufferHeap==BufferHeap::Readback) {
    // heapDesc.Properties.Type = D3D12_HEAP_TYPE_READBACK;
    heapDesc.Properties.Type                 = D3D12_HEAP_TYPE_CUSTOM;
    heapDesc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
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

DxBuffer DxAllocator::alloc(const void* mem, size_t size, MemUsage usage, BufferHeap bufFlg) {
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

  if(MemUsage::UniformBuffer==(usage&MemUsage::UniformBuffer) && resDesc.Width%256!=0) {
    // CB size is required to be 256-byte aligned.
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-descriptors#constant-buffer-view
    resDesc.Width += 256-resDesc.Width%256;
    }

  if(MemUsage::StorageBuffer==(usage&MemUsage::StorageBuffer) && resDesc.Width%256!=0) {
    // 'readonly buffer' in glsl, expressed as D3D12_CONSTANT_BUFFER_VIEW_DESC, not as UAV
    resDesc.Width += 256-resDesc.Width%256;
    }

  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
  if(MemUsage::StorageBuffer==(usage&MemUsage::StorageBuffer)) {
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    // state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
  if(MemUsage::AsStorage==(usage&MemUsage::AsStorage)) {
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    state         |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }
  if(MemUsage::Indirect==(usage&MemUsage::Indirect)) {
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    // state          = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT | D3D12_RESOURCE_STATE_GENERIC_READ | D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
  if(MemUsage::ScratchBuffer==(usage&MemUsage::ScratchBuffer)) {
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    state          = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

  DxBuffer ret(owner,UINT(resDesc.Width), UINT(size));
  ret.page = allocator.alloc(size, toAlignment(bufFlg),
                             uint32_t(bufFlg), uint32_t(bufFlg), (bufFlg!=BufferHeap::Device));
  if(!ret.page.page)
    throw std::system_error(Tempest::GraphicsErrc::OutOfVideoMemory);

  if(!commit(ret.page.page->memory,ret.page.page->mmapSync,ret.impl.get(),
             resDesc, state, ret.page.offset,
             mem,size)) {
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }

  if( MemUsage::StorageBuffer==(usage&MemUsage::StorageBuffer) ||
      MemUsage::TransferDst  ==(usage&MemUsage::TransferDst) ||
      MemUsage::AsStorage    ==(usage&MemUsage::AsStorage)) {
    ret.nonUniqId = nextId();
    }
  return ret;
  }

DxTexture DxAllocator::alloc(const Pixmap& pm, uint32_t mip, DXGI_FORMAT format) {
  ComPtr<ID3D12Resource> ret;

  D3D12_RESOURCE_DESC1 resDesc = {};
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
  resDesc.SamplerFeedbackMipRegion = {};

  D3D12_HEAP_PROPERTIES heapProp={};
  heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProp.CreationNodeMask     = 1;
  heapProp.VisibleNodeMask      = 1;

  if(owner->props.enhancedBarriers) {
    ComPtr<ID3D12Device10> dev10;
    //device->QueryInterface(uuid<ID3D12Device10>(), reinterpret_cast<void**>(&dev10));
    device->QueryInterface(_uuidof(ID3D12Device10), reinterpret_cast<void**>(&dev10));
    dxAssert(dev10->CreateCommittedResource3(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_BARRIER_LAYOUT_COPY_DEST,
        nullptr,
        nullptr,
        0, nullptr,
        uuid<ID3D12Resource>(),
        reinterpret_cast<void**>(&ret)
        ));
    } else {
    dxAssert(device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        reinterpret_cast<const D3D12_RESOURCE_DESC*>(&resDesc),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        uuid<ID3D12Resource>(),
        reinterpret_cast<void**>(&ret)
        ));
    }
  return DxTexture(std::move(ret),resDesc.Format,NonUniqResId::I_None,resDesc.MipLevels,1,true);
  }

DxTexture DxAllocator::alloc(const uint32_t w, const uint32_t h, const uint32_t d, const uint32_t mip, TextureFormat frm, bool imageStore) {
  D3D12_RESOURCE_STATES state  = D3D12_RESOURCE_STATE_COMMON;
  D3D12_BARRIER_LAYOUT  layout = D3D12_BARRIER_LAYOUT_COMMON;
  D3D12_CLEAR_VALUE     clr    = {};

  D3D12_RESOURCE_DESC1 resDesc = {}; // binary compatible with D3D12_RESOURCE_DESC, so it's OK
  resDesc.MipLevels          = mip;
  resDesc.Format             = Detail::nativeFormat(frm);
  resDesc.Width              = w;
  resDesc.Height             = h;
  resDesc.DepthOrArraySize   = std::max<uint32_t>(d, 1);
  if(isDepthFormat(frm)) {
    state                    = D3D12_RESOURCE_STATE_DEPTH_READ;
    layout                   = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
    resDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    clr.DepthStencil.Depth   = 1.f;
    clr.DepthStencil.Stencil = 0;
    } else {
    state                    = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    layout                   = D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
    resDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    clr.Color[0] = 0.f;
    clr.Color[1] = 0.f;
    clr.Color[2] = 0.f;
    clr.Color[3] = 0.f;
    }
  if(imageStore) {
    state          = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    layout         = D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
    resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
  resDesc.SampleDesc.Count   = 1;
  resDesc.SampleDesc.Quality = 0;
  resDesc.Dimension          = d==0 ? D3D12_RESOURCE_DIMENSION_TEXTURE2D : D3D12_RESOURCE_DIMENSION_TEXTURE3D;
  resDesc.SamplerFeedbackMipRegion = {};

  D3D12_HEAP_PROPERTIES heapProp={};
  heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProp.CreationNodeMask     = 1;
  heapProp.VisibleNodeMask      = 1;

  clr.Format = resDesc.Format;

  ComPtr<ID3D12Resource> ret;
  if(owner->props.enhancedBarriers) {
    ComPtr<ID3D12Device10> dev10;
    //device->QueryInterface(uuid<ID3D12Device10>(), reinterpret_cast<void**>(&dev10));
    device->QueryInterface(_uuidof(ID3D12Device10), reinterpret_cast<void**>(&dev10));

    dxAssert(dev10->CreateCommittedResource3(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        layout,
        &clr,
        nullptr,
        0, nullptr,
        uuid<ID3D12Resource>(),
        reinterpret_cast<void**>(&ret)
        ));
    } else {
    dxAssert(device->CreateCommittedResource(
               &heapProp,
               D3D12_HEAP_FLAG_NONE,
               reinterpret_cast<const D3D12_RESOURCE_DESC*>(&resDesc),
               state,
               &clr,
               uuid<ID3D12Resource>(),
               reinterpret_cast<void**>(&ret)
               ));
    }

  D3D12_FEATURE_DATA_FORMAT_SUPPORT ds = {};
  ds.Format = resDesc.Format;
  if(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &ds, sizeof(ds)))) {
    // nop
    }
  const auto nonUniqId  = (imageStore) ?  nextId() : NonUniqResId::I_None;
  const bool filterable = (ds.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
  return DxTexture(std::move(ret),resDesc.Format,nonUniqId,resDesc.MipLevels,resDesc.DepthOrArraySize,filterable);
  }

void DxAllocator::free(Allocation& page) {
  if(page.page!=nullptr)
    allocator.free(page);
  }

bool DxAllocator::commit(ID3D12Heap* heap, std::mutex& mmapSync,
                         ID3D12Resource*& dest, const D3D12_RESOURCE_DESC& resDesc, D3D12_RESOURCE_STATES state,
                         size_t offset, const void* mem, size_t size) {
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
    std::memcpy(mapped, mem, size);
    dest->Unmap(0,&rgn);
    }
  return true;
  }

#endif
