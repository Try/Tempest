#if defined(TEMPEST_BUILD_VULKAN)

#include "vresourceheap.h"

#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

void VResourceHeap::Provider::realloc(uint32_t size) {
  auto buf  = dev->allocator.alloc(nullptr, size, MemUsage::Descriptor, BufferHeap::Upload);
  auto next = DSharedPtr<VDescriptorHeap*>(new VDescriptorHeap(std::move(buf)));

  std::lock_guard<SpinLock> guard(sync);
  if(memory.handler==nullptr) {
    memory = next;
    return;
    }
  memory.handler->flush();

  auto& props   = dev->props;
  auto  elSize  = props.resourceDescriptorSize;
  auto  reserve = ((props.resourceHeapReserve + elSize - 1) / elSize) * elSize;
  std::memcpy(next.handler->hptr+reserve, memory.handler->hptr+reserve, memory.handler->size()-reserve);
  memory = next;
  }

void VResourceHeap::Provider::flush() {
  std::lock_guard<SpinLock> guard(sync);
  if(memory)
    memory.handler->flush();
  }


VResourceHeap::VResourceHeap() : allocator(provider) {
  }

VResourceHeap::~VResourceHeap() {
  }

void VResourceHeap::setDevice(VDevice& dx) {
  provider.dev         = &dx;
  provider.elementSize = dx.props.resourceDescriptorSize;
  provider.reserveSize = dx.props.resourceHeapReserve;
  provider.maxSize     = dx.props.resourceHeapMaxSize;
  }

VResourceHeap::Allocation VResourceHeap::alloc(AbstractGraphicsApi::Buffer** buf, size_t cnt) {
  auto ret = allocator.alloc(uint32_t(cnt), [&](auto alloc) {
    auto  dPtrR = alloc.ptr;
    auto  hPtrR = provider.memory ? provider.memory.handler->hptr : nullptr;

    for(size_t i=0; i<cnt; ++i) {
      auto res = hPtrR + (dPtrR + i)*provider.elementSize;
      VPushDescriptor::write(*provider.dev, res, ShaderReflection::SsboR, buf[i], 0, ComponentMapping());
      }
    });

  return Allocation(ret.ptr);
  }

VResourceHeap::Allocation VResourceHeap::alloc(AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel) {
  auto ret = allocator.alloc(uint32_t(cnt), [&](auto alloc) {
    auto  dPtrR = alloc.ptr;
    auto  hPtrR = provider.memory ? provider.memory.handler->hptr : nullptr;

    for(size_t i=0; i<cnt; ++i) {
      auto res = hPtrR + (dPtrR + i)*provider.elementSize;
      VPushDescriptor::write(*provider.dev, res, ShaderReflection::Image, tex[i], mipLevel, ComponentMapping());
      }
    });

  return Allocation(ret.ptr);
  }

VResourceHeap::Allocation VResourceHeap::alloc(uint32_t num) {
  auto ret = allocator.alloc(uint32_t(num));
  return Allocation(ret.ptr);
  }

void VResourceHeap::flush() {
  if(provider.dev==nullptr)
    return;
  allocator.flush();
  }

DSharedPtr<VDescriptorHeap*> VResourceHeap::currentMemory() const {
  std::lock_guard<SpinLock> guard(provider.sync);
  return provider.memory;
  }

void VResourceHeap::free(uint32_t ptr, uint32_t num) {
  allocator.free(ptr, num);
  return;
  }

#endif