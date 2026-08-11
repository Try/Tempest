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


struct VDescriptorHeap::VHeap : VBuffer {
  VHeap(VBuffer&& v)  :VBuffer(std::move(v)) {
    hptr = this->mapDescriptorHeap();
    }

  ~VHeap() {
    unmapDescriptorHeap();
    }

  uint8_t* hptr = nullptr;
  };

VDescriptorHeap::VDescriptorHeap() {
  }

VDescriptorHeap::~VDescriptorHeap() {
  }

void VDescriptorHeap::setDevice(VDevice& dx) {
  dev = &dx;
  rgn.reserve(1024);
  }

VDescriptorHeap::Allocation VDescriptorHeap::alloc(AbstractGraphicsApi::Buffer** buf, size_t cnt) {
  std::lock_guard<std::recursive_mutex> guard(sync);

  auto& props = dev->props;
  auto  alloc = this->alloc(cnt);
  auto  dPtrR = alloc.ptr;

  for(size_t i=0; i<cnt; ++i) {
    auto res = alloc.hptr;
    res += (dPtrR + i)*props.resourceDescriptorSize;
    VPushDescriptor::write(*dev, res, nullptr, ShaderReflection::SsboR, buf[i], 0, ComponentMapping(), Sampler::nearest());
    }

  return alloc;
  }

VDescriptorHeap::Allocation VDescriptorHeap::alloc(AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel) {
  std::lock_guard<std::recursive_mutex> guard(sync);

  auto& props = dev->props;
  auto  alloc = this->alloc(cnt);
  auto  dPtrR = alloc.ptr;

  for(size_t i=0; i<cnt; ++i) {
    auto res = alloc.hptr;
    res += (dPtrR + i)*props.resourceDescriptorSize;
    VPushDescriptor::write(*dev, res, nullptr, ShaderReflection::Image, tex[i], mipLevel, ComponentMapping(), Sampler::nearest());
    }

  return alloc;
  }

void VDescriptorHeap::flush() {
  if(dev==nullptr)
    return;
  if(memory!=nullptr)
    dev->allocator.flushDescriptorHeap(*memory);
  }

VDescriptorHeap::Allocation VDescriptorHeap::alloc(uint32_t num) {
  std::lock_guard<std::recursive_mutex> guard(sync);

  auto& props   = dev->props;
  auto  maxSize = props.resourceHeapMaxSize;
  auto  elSize  = props.resourceDescriptorSize;
  auto  reserve = ((props.resourceHeapReserve + elSize - 1) / elSize) * elSize;

  const uint32_t allocSize = num*elSize;

  for(size_t i=0; i<rgn.size(); ++i) {
    auto& r = rgn[i];
    if(r.begin+allocSize > r.end)
      continue;

    uint32_t ret = r.begin;
    r.begin += allocSize;
    if(r.begin==r.end)
      rgn.erase(rgn.begin() + i);
    return Allocation{ret/elSize, memory->hptr, memory};
    }

  // realloc heap
  auto prevSize = uint32_t(memory ? memory->size() : 0);
  auto size     = uint32_t(memory ? memory->size() : reserve) + allocSize;
  if(size > maxSize)
    throw std::bad_alloc();

  size = std::min(std::max(nextPot(size), 4*1024u), maxSize);
  auto buf  = dev->allocator.alloc(nullptr, size, MemUsage::Descriptor, BufferHeap::Upload);
  auto next = std::make_shared<VHeap>(std::move(buf));

  if(memory==nullptr) {
    Range rg;
    rg.begin = reserve + allocSize;
    rg.end   = size;

    rgn.push_back(rg);
    memory = next;
    return Allocation{uint32_t(reserve/elSize), memory->hptr, memory};
    }

  dev->allocator.flushDescriptorHeap(*memory);
  std::memcpy(next->hptr+reserve, memory->hptr+reserve, memory->size()-reserve);

  Range& rg = rgn.emplace_back();
  rg.begin = prevSize + allocSize;
  rg.end   = uint32_t(next->size());
  memory = next;
  return Allocation{uint32_t(prevSize/elSize), memory->hptr, memory};
  }

void VDescriptorHeap::free(uint32_t ptr, uint32_t num) {
  if(ptr==0xFFFFFFFF || num==0)
    return;

  std::lock_guard<std::recursive_mutex> guard(sync);

  auto elSize = dev->props.resourceDescriptorSize;
  ptr *= elSize;
  num *= elSize;

  size_t i = 0;
  for(; i+1<rgn.size(); ++i) {
    auto& r = rgn[i+1];
    if(r.end>=ptr)
      break;
    }

  for(; i<rgn.size(); ++i) {
    auto& r = rgn[i];
    if(ptr+num==r.begin) {
      r.begin -= num;
      return;
      }
    if(ptr+num<r.begin) {
      Range r = {ptr, ptr+num};
      rgn.insert(rgn.begin() + i, r);
      return;
      }
    }

  // bad free
  throw std::bad_alloc();
  }

#endif