#if defined(TEMPEST_BUILD_VULKAN)

#include "vdescriptorheap.h"

#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

static uint32_t nextPot(uint32_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
  }


VDescriptorHeap::VHeap::VHeap(VBuffer&& v)
  :VBuffer(std::move(v)) {
  hptr = this->mapDescriptorHeap();
  }

VDescriptorHeap::VHeap::~VHeap() {
  unmapDescriptorHeap();
  }


VDescriptorHeap::VDescriptorHeap() {
  }

VDescriptorHeap::~VDescriptorHeap() {
  }

void VDescriptorHeap::setDevice(VDevice& dx) {
  dev = &dx;
  allocator[0].rgn.reserve(1024);
  allocator[1].rgn.reserve(1024);
  }

VDescriptorHeap::Allocation VDescriptorHeap::alloc(AbstractGraphicsApi::Buffer** buf, size_t cnt) {
  std::lock_guard<std::recursive_mutex> guard(allocator[HEAP_TYPE_CBV_SRV_UAV].sync);

  auto& props = dev->props;
  auto  alloc = this->alloc(HEAP_TYPE_CBV_SRV_UAV, cnt);
  auto  dPtrR = alloc.ptr;

  for(size_t i=0; i<cnt; ++i) {
    auto res = alloc.hptr;
    res += (dPtrR + i)*props.resourceDescriptorSize;
    VPushDescriptor::write(*dev, res, nullptr, ShaderReflection::SsboR, buf[i], 0, ComponentMapping(), Sampler::nearest());
    }

  return alloc;
  }

VDescriptorHeap::Allocation VDescriptorHeap::alloc(AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel) {
  std::lock_guard<std::recursive_mutex> guard(allocator[HEAP_TYPE_CBV_SRV_UAV].sync);

  auto& props = dev->props;
  auto  alloc = this->alloc(HEAP_TYPE_CBV_SRV_UAV, cnt);
  auto  dPtrR = alloc.ptr;

  for(size_t i=0; i<cnt; ++i) {
    auto res = alloc.hptr;
    res += (dPtrR + i)*props.resourceDescriptorSize;
    VPushDescriptor::write(*dev, res, nullptr, ShaderReflection::Image, tex[i], mipLevel, ComponentMapping(), Sampler::nearest());
    }

  return alloc;
  }

VDescriptorHeap::Allocation VDescriptorHeap::alloc(HeapType heapType, uint32_t num) {
  return alloc(allocator[heapType], heapType, num);
  }

void VDescriptorHeap::free(HeapType heap, uint32_t ptr, uint32_t num) {
  if(ptr==0xFFFFFFFF || num==0)
    return;
  free(allocator[heap], ptr, num);
  }

void VDescriptorHeap::flush() {
  if(dev==nullptr)
    return;
  for(auto& i:allocator)
    if(i.memory!=nullptr)
      dev->allocator.flushDescriptorHeap(*i.memory);
  }

VDescriptorHeap::Allocation VDescriptorHeap::alloc(Allocator& heap, HeapType heapType, uint32_t num) {
  std::lock_guard<std::recursive_mutex> guard(heap.sync);

  for(size_t i=0; i<heap.rgn.size(); ++i) {
    auto& r = heap.rgn[i];
    if(r.begin+num > r.end)
      continue;

    uint32_t ret = r.begin;
    r.begin += num;
    if(r.begin==r.end)
      heap.rgn.erase(heap.rgn.begin() + i);
    return Allocation{ret, heap.ptr, heap.memory};
    }

  // realloc heap
  auto& props   = dev->props;
  auto  reserve = (heapType==HEAP_TYPE_CBV_SRV_UAV) ? props.resourceHeapReserve    : props.samplerHeapReserve;
  auto  maxSize = (heapType==HEAP_TYPE_CBV_SRV_UAV) ? props.resourceHeapMaxSize    : props.samplerHeapMaxSize;
  auto  elSize  = (heapType==HEAP_TYPE_CBV_SRV_UAV) ? props.resourceDescriptorSize : props.samplerDescriptorSize;
  auto  prevNum = (heap.memory ? (heap.memory->size()-reserve) : 0) / elSize;

  auto size = (heap.memory ? heap.memory->size() : reserve) + num*elSize;
  if(size > maxSize)
    throw std::bad_alloc();
  size = std::min(std::max(nextPot(size), 16*1024u), maxSize);

  auto buf   = dev->allocator.alloc(nullptr, size, MemUsage::Descriptor, BufferHeap::Upload);
  auto pnext = std::make_shared<VHeap>(std::move(buf));
  auto ptr   = pnext->hptr;
  if(ptr==nullptr)
    throw std::bad_alloc();

  if(heap.memory!=nullptr) {
    std::memcpy(ptr, heap.ptr, heap.memory->size()-reserve);
    }

  Range rgn;
  rgn.begin = prevNum + num;
  rgn.end   = uint32_t(size-reserve)/elSize;

  heap.rgn.push_back(rgn);
  heap.memory = pnext;
  heap.ptr    = ptr;
  return Allocation{uint32_t(prevNum), heap.ptr, heap.memory};
  }

void VDescriptorHeap::free(Allocator& heap, uint32_t ptr, uint32_t num) {
  std::lock_guard<std::recursive_mutex> guard(heap.sync);

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