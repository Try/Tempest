#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxheapallocator.h"

#include "dxdevice.h"
#include "guid.h"
#include <d3d12.h>

using namespace Tempest;
using namespace Tempest::Detail;

DxHeapAllocator::DxHeapAllocator(DxDevice& d)
  : dev(&d) {
  auto& device = *dev->device.get();
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

  D3D12_DESCRIPTOR_HEAP_DESC dRes = {};
  dRes.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  dRes.NumDescriptors = UINT(maxRes);
  dRes.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  dxAssert(device.CreateDescriptorHeap(&dRes, ::uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&resHeap)));

  dRes.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  dRes.NumDescriptors = UINT(maxSmp);
  dRes.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  dxAssert(device.CreateDescriptorHeap(&dRes, ::uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&smpHeap)));

  allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].rgn.reserve(maxRes/64);
  allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].rgn.push_back(Range{0, maxRes});

  allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].rgn.reserve(maxSmp);
  allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].rgn.push_back(Range{0, maxSmp});

  resSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  smpSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  res = resHeap->GetCPUDescriptorHandleForHeapStart();
  smp = smpHeap->GetCPUDescriptorHandleForHeapStart();

  gres = resHeap->GetGPUDescriptorHandleForHeapStart();
  gsmp = smpHeap->GetGPUDescriptorHandleForHeapStart();
  }

uint32_t DxHeapAllocator::alloc(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t num) {
  return alloc(allocator[heap], num);
  }

void DxHeapAllocator::free(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t ptr, uint32_t num) {
  if(ptr==0xFFFFFFFF || num==0)
    return;
  free(allocator[heap], ptr, num);
  }

uint32_t DxHeapAllocator::alloc(Allocator& heap, uint32_t num) {
  std::lock_guard<std::mutex> guard(heap.sync);

  for(size_t i=0; i<heap.rgn.size(); ++i) {
    auto& r = heap.rgn[i];
    if(r.begin+num > r.end)
      continue;

    uint32_t ret = r.begin;
    r.begin += num;
    return ret;
    }

  throw std::bad_alloc();
  }

void DxHeapAllocator::free(Allocator& heap, uint32_t ptr, uint32_t num) {
  std::lock_guard<std::mutex> guard(heap.sync);

  size_t i = 0;
  for(; i+1<heap.rgn.size(); ++i) {
    auto& r = heap.rgn[i+1];
    if(r.end>=ptr)
      break;
    }

  for(; i<heap.rgn.size(); ++i) {
    auto& r = heap.rgn[i];
    if(ptr+num==r.begin) {
      r.begin -= num;
      return;
      }
    if(ptr+num<r.begin) {
      Range r = {ptr, ptr+num};
      heap.rgn.insert(heap.rgn.begin() + i, r);
      return;
      }
    }

  // bad free
  throw std::bad_alloc();
  }

#endif
