#if defined(TEMPEST_BUILD_VULKAN)

#include "vresourceheap.h"

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


VResourceHeap::VResourceHeap() {
  }

VResourceHeap::~VResourceHeap() {
  }

void VResourceHeap::setDevice(VDevice& dx) {
  dev = &dx;
  rgn.reserve(1024);
  }

VResourceHeap::Allocation VResourceHeap::alloc(AbstractGraphicsApi::Buffer** buf, size_t cnt) {
  std::unique_lock<std::shared_mutex> guard(sync);

  auto& props = dev->props;
  auto  alloc = implAlloc(uint32_t(cnt), true);
  auto  dPtrR = alloc.ptr;
  auto  hPtrR = memory ? memory.handler->hptr : nullptr;

  for(size_t i=0; i<cnt; ++i) {
    auto res = hPtrR + (dPtrR + i)*props.resourceDescriptorSize;
    VPushDescriptor::write(*dev, res, ShaderReflection::SsboR, buf[i], 0, ComponentMapping());
    }

  return alloc;
  }

VResourceHeap::Allocation VResourceHeap::alloc(AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel) {
  std::unique_lock<std::shared_mutex> guard(sync);

  auto& props = dev->props;
  auto  alloc = implAlloc(uint32_t(cnt), true);
  auto  dPtrR = alloc.ptr;
  auto  hPtrR = memory ? memory.handler->hptr : nullptr;

  for(size_t i=0; i<cnt; ++i) {
    auto res = hPtrR + (dPtrR + i)*props.resourceDescriptorSize;
    VPushDescriptor::write(*dev, res, ShaderReflection::Image, tex[i], mipLevel, ComponentMapping());
    }

  return alloc;
  }

VResourceHeap::Allocation VResourceHeap::alloc(uint32_t num) {
  {
    std::shared_lock<std::shared_mutex> guard(sync);
    auto ret = implAlloc(num, false);
    if(ret.ptr!=uint32_t(-1))
      return ret;
  }
  std::unique_lock<std::shared_mutex> guard(sync);
  return implAlloc(num, true);
  }

void VResourceHeap::flush() {
  if(dev==nullptr)
    return;
  std::shared_lock<std::shared_mutex> guard(sync);
  if(memory)
    memory.handler->flush();
  }

DSharedPtr<VDescriptorHeap*> VResourceHeap::currentMemory() const {
  std::shared_lock<std::shared_mutex> guard(sync);
  return memory;
  }

VResourceHeap::Allocation VResourceHeap::implAlloc(uint32_t num, bool allowRealloc) {
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
    return Allocation{ret/elSize};
    }

  if(!allowRealloc)
    return Allocation{uint32_t(-1)};

  // realloc heap
  auto prevSize = uint32_t(memory ? memory.handler->size() : 0);
  auto size     = uint32_t(memory ? memory.handler->size() : reserve) + allocSize;
  if(size > maxSize)
    throw std::bad_alloc();

#if 1
  size = std::min(std::max(nextPot(size), 4*1024u), maxSize);
#else
  size = std::min(std::max(nextPot(size), 1024*1024u), maxSize);
#endif
  auto buf  = dev->allocator.alloc(nullptr, size, MemUsage::Descriptor, BufferHeap::Upload);
  auto next = DSharedPtr<VDescriptorHeap*>(new VDescriptorHeap(std::move(buf)));

  if(!memory) {
    Range rg;
    rg.begin = reserve + allocSize;
    rg.end   = size;

    rgn.push_back(rg);
    memory = next;
    // std::memset(next->hptr, 0xDEADBEEF, next->size());
    return Allocation{uint32_t(reserve/elSize)};
    }

  memory.handler->flush();
  std::memcpy(next.handler->hptr+reserve, memory.handler->hptr+reserve, memory.handler->size()-reserve);

  Range& rg = rgn.emplace_back();
  rg.begin = prevSize + allocSize;
  rg.end   = uint32_t(next.handler->size());
  memory = next;
  return Allocation{uint32_t(prevSize/elSize)};
  }

void VResourceHeap::free(uint32_t ptr, uint32_t num) {
  if(ptr==0xFFFFFFFF || num==0)
    return;

  std::shared_lock<std::shared_mutex> guard(sync);

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