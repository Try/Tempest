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


void DxDescriptorAllocator::ProviderHeap::realloc(uint32_t size) {
  auto& device = *dev->device.get();

  D3D12_DESCRIPTOR_HEAP_DESC dRes = {};
  dRes.Type           = type;
  dRes.NumDescriptors = UINT(maxSize/elementSize);
  dRes.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  dxAssert(device.CreateDescriptorHeap(&dRes, ::uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&memory)));

  memSize = maxSize;
  cpu     = memory->GetCPUDescriptorHandleForHeapStart();
  gpu     = memory->GetGPUDescriptorHandleForHeapStart();
  }


DxDescriptorAllocator::DxDescriptorAllocator() {
  }

void DxDescriptorAllocator::setDevice(DxDevice& d) {
  auto& device = *d.device.get();
  UINT maxSmp = 2048;
  UINT maxRes = 1'000'000;

  // https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support
  D3D12_FEATURE_DATA_D3D12_OPTIONS feature0 = {};
  if(SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &feature0, sizeof(feature0)))) {
    switch(feature0.ResourceBindingTier) {
      case D3D12_RESOURCE_BINDING_TIER_1:
        maxSmp = 16;
        maxRes = 1,000,000;
        break;
      case D3D12_RESOURCE_BINDING_TIER_2:
        maxSmp = 2048;
        maxRes = 1,000,000;
        break;
      case D3D12_RESOURCE_BINDING_TIER_3:
        maxSmp = 2048;
        maxRes = 1'000'000; // or more
        break;
      }
    }

  providerRes.dev         = &d;
  providerRes.type        = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  providerRes.elementSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  providerRes.maxSize     = maxRes * providerRes.elementSize;

  providerSmp.dev         = &d;
  providerSmp.type        = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  providerSmp.elementSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
  providerSmp.maxSize     = maxSmp * providerSmp.elementSize;

  providerHost.device     = &d;
  allocatorHost.setDefaultPageSize(64);

  resSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  smpSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
  rtvSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  dsvSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  }

DxDescriptorAllocator::Allocation DxDescriptorAllocator::allocDsv(size_t count) {
  if(count==0)
    return Allocation();
  const auto tid = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  auto ret = allocatorHost.alloc(count, 1, tid, tid, true);
  return ret;
  }

DxDescriptorAllocator::Allocation DxDescriptorAllocator::allocRtv(size_t count) {
  if(count==0)
    return Allocation();
  const auto tid = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  auto ret = allocatorHost.alloc(count, 1, tid, tid, true);
  return ret;
  }

DxDescriptorAllocator::Allocation DxDescriptorAllocator::allocHost(size_t count) {
  if(count==0)
    return Allocation();
  const auto tid = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  auto ret = allocatorHost.alloc(count, 1, tid, tid, true);
  return ret;
  }

uint32_t DxDescriptorAllocator::allocRes(uint32_t num) {
  return allocatorRes.alloc(num).ptr;
  }

uint32_t DxDescriptorAllocator::allocSmp(uint32_t num) {
  return allocatorSmp.alloc(num).ptr;
  }

void DxDescriptorAllocator::free(Allocation& page) {
  if(page.page!=nullptr) {
    auto& allocator = allocatorHost;
    allocator.free(page);
    }
  }

ID3D12DescriptorHeap* DxDescriptorAllocator::currentMemory(D3D12_DESCRIPTOR_HEAP_TYPE heap) const {
  if(heap==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    return providerRes.memory.get();
  if(heap==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    return providerSmp.memory.get();
  return nullptr;
  }

D3D12_CPU_DESCRIPTOR_HANDLE DxDescriptorAllocator::handleCpu(const Allocation& a, size_t offset) {
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

D3D12_CPU_DESCRIPTOR_HANDLE DxDescriptorAllocator::handleCpu(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t offset) {
  D3D12_CPU_DESCRIPTOR_HANDLE ret = {};
  switch (heap) {
    case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
      ret = providerRes.cpu;
      ret.ptr += offset*resSize;
      break;
    case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
      ret = providerSmp.cpu;
      ret.ptr += offset*smpSize;
      break;
    default:
      break;
    }
  return ret;
  }

D3D12_GPU_DESCRIPTOR_HANDLE DxDescriptorAllocator::handleGpu(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t offset) {
  D3D12_GPU_DESCRIPTOR_HANDLE ret = {};
  switch (heap) {
    case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
      ret = providerRes.gpu;
      ret.ptr += offset*resSize;
      break;
    case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
      ret = providerSmp.gpu;
      ret.ptr += offset*smpSize;
      break;
    default:
      break;
    }
  return ret;
  }

uint32_t DxDescriptorAllocator::alloc(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t num) {
  if(heap==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    return allocatorRes.alloc(num).ptr;
  if(heap==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    return allocatorSmp.alloc(num).ptr;
  throw std::bad_alloc();
  }

void DxDescriptorAllocator::free(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t ptr, uint32_t num) {
  if(heap==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    allocatorRes.free(ptr, num);
  if(heap==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    allocatorSmp.free(ptr, num);
  }

#endif
