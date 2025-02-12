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
  descSize = dx.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  smpSize  = dx.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  allocatorRes.setDefaultPageSize(64);
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

D3D12_CPU_DESCRIPTOR_HANDLE DxDescriptorAllocator::handle(const Allocation& a) {
  D3D12_CPU_DESCRIPTOR_HANDLE ptr = {};
  if(a.page==nullptr)
    return ptr;

  if(a.page->memory!=nullptr)
    ptr = a.page->memory->GetCPUDescriptorHandleForHeapStart();

  if(a.page->heapId==0)
    ptr.ptr += a.offset*descSize; else
    ptr.ptr += a.offset*smpSize;

  return ptr;
  }

D3D12_GPU_DESCRIPTOR_HANDLE DxDescriptorAllocator::gpuHandle(const Allocation& a) {
  D3D12_GPU_DESCRIPTOR_HANDLE ptr = {};
  if(a.page==nullptr)
    return ptr;

  if(a.page->memory!=nullptr)
    ptr = a.page->memory->GetGPUDescriptorHandleForHeapStart();

  if(a.page->heapId==0)
    ptr.ptr += a.offset*descSize; else
    ptr.ptr += a.offset*smpSize;

  return ptr;
  }

#endif
