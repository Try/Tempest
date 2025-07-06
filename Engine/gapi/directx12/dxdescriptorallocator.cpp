#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxdescriptorallocator.h"

#include "dxdevice.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxDescriptorAllocator::Provider::~Provider() {
  if(last!=nullptr)
    last->Release();
  }

DxDescriptorAllocator::Provider::DeviceMemory DxDescriptorAllocator::Provider::alloc(size_t size, uint32_t typeId) {
  if(last!=nullptr) {
    if(lastTypeId==typeId && lastSize==size){
      auto ret = last;
      last = nullptr;
      return ret;
      }
    last->Release();
    last = nullptr;
    }

  D3D12_DESCRIPTOR_HEAP_DESC d = {};
  d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE(typeId);
  d.NumDescriptors = UINT(size);
  d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

  ID3D12DescriptorHeap* ret = nullptr;
  HRESULT hr = device->device->CreateDescriptorHeap(&d, ::uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&ret));
  if(SUCCEEDED(hr))
    return ret;
  return nullptr;
  }

void DxDescriptorAllocator::Provider::free(DeviceMemory m, size_t size, uint32_t typeId) {
  if(last!=nullptr)
    last->Release();
  last       = m;
  lastSize   = size;
  lastTypeId = typeId;
  }


DxDescriptorAllocator::DxDescriptorAllocator() {
  }

void DxDescriptorAllocator::setDevice(DxDevice& device) {
  providerRes.device = &device;

  auto& dx = *device.device.get();
  resSize = dx.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  rtvSize = dx.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  dsvSize = dx.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

  allocatorRes.setDefaultPageSize(64);
  }

DxDescriptorAllocator::Allocation DxDescriptorAllocator::allocDsv(size_t count) {
  if(count==0)
    return Allocation();
  const auto tid = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  auto ret = allocatorRes.alloc(count, 1, tid, tid, true);
  return ret;
  }

DxDescriptorAllocator::Allocation DxDescriptorAllocator::allocRtv(size_t count) {
  if(count==0)
    return Allocation();
  const auto tid = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  auto ret = allocatorRes.alloc(count, 1, tid, tid, true);
  return ret;
  }

DxDescriptorAllocator::Allocation DxDescriptorAllocator::allocHost(size_t count) {
  if(count==0)
    return Allocation();
  const auto tid = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  auto ret = allocatorRes.alloc(count, 1, tid, tid, true);
  return ret;
  }

void DxDescriptorAllocator::free(Allocation& page) {
  if(page.page!=nullptr) {
    auto& allocator = allocatorRes;
    allocator.free(page);
    }
  }

ID3D12DescriptorHeap* DxDescriptorAllocator::heapof(const Allocation& a) {
  return a.page!=nullptr ? a.page->memory : nullptr;
  }

D3D12_CPU_DESCRIPTOR_HANDLE DxDescriptorAllocator::handle(const Allocation& a, size_t offset) {
  D3D12_CPU_DESCRIPTOR_HANDLE ptr = {};
  if(a.page==nullptr)
    return ptr;

  if(a.page->memory!=nullptr)
    ptr = a.page->memory->GetCPUDescriptorHandleForHeapStart();

  offset += a.offset;

  switch (a.page->heapId) {
    case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
      ptr.ptr += offset*resSize;
      break;
    case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
      assert(0); // not in use
      break;
    case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
      ptr.ptr += offset*rtvSize;
      break;
    case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
      ptr.ptr += offset*dsvSize;
      break;
    }

  return ptr;
  }

#endif
