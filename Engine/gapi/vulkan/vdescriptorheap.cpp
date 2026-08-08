#if defined(TEMPEST_BUILD_VULKAN)

#include "vdescriptorheap.h"

#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VDescriptorHeap::VDescriptorHeap() {
  }

VDescriptorHeap::~VDescriptorHeap() {
  if(dev==nullptr)
    return;
  dev->allocator.unmapDescriptorHeap(resources);
  dev->allocator.unmapDescriptorHeap(samplers);
  }

void VDescriptorHeap::setDevice(VDevice& dx) {
  dev = &dx;

  auto& props = dev->props;
  const auto maxRes = (props.resourceHeapMaxSize - props.resourceHeapReserve)/props.resourceDescriptorSize;
  const auto maxSmp = (props.samplerHeapMaxSize  - props.samplerHeapReserve )/props.samplerDescriptorSize;

  VkDeviceSize heapSizeResources = props.resourceDescriptorSize*65*1024 + props.resourceHeapReserve;
  VkDeviceSize heapSizeSamplers  = props.samplerDescriptorSize *1024    + props.samplerHeapReserve;

  resources    = dev->allocator.alloc(nullptr, heapSizeResources, MemUsage::Descriptor, BufferHeap::Upload);
  resourcesPtr = dev->allocator.mapDescriptorHeap(resources);

  samplers    = dev->allocator.alloc(nullptr, heapSizeSamplers,  MemUsage::Descriptor, BufferHeap::Upload);
  samplersPtr = dev->allocator.mapDescriptorHeap(samplers);

  allocator[0].rgn.reserve(1024);
  allocator[0].rgn.push_back(Range{0, uint32_t(65*1024)});

  allocator[1].rgn.reserve(1024);
  allocator[1].rgn.push_back(Range{0, uint32_t(1024)});
  }

uint32_t VDescriptorHeap::alloc(HeapType heap, uint32_t num) {
  return alloc(allocator[heap], num);
  }

void VDescriptorHeap::free(HeapType heap, uint32_t ptr, uint32_t num) {
  if(ptr==0xFFFFFFFF || num==0)
    return;
  free(allocator[heap], ptr, num);
  }

uint32_t VDescriptorHeap::alloc(Allocator& heap, uint32_t num) {
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

void VDescriptorHeap::free(Allocator& heap, uint32_t ptr, uint32_t num) {
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