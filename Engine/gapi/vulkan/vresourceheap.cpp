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


VResourceHeap::VResourceHeap()  {
  }

VResourceHeap::~VResourceHeap() {
  }

void VResourceHeap::setDevice(VDevice& dx) {
  providerRes.dev         = &dx;
  providerRes.elementSize = dx.props.resourceDescriptorSize;
  providerRes.reserveSize = dx.props.resourceHeapReserve;
  providerRes.maxSize     = dx.props.resourceHeapMaxSize;

  providerSmp.dev         = &dx;
  providerSmp.elementSize = dx.props.samplerDescriptorSize;
  providerSmp.reserveSize = dx.props.samplerHeapReserve;
  providerSmp.maxSize     = dx.props.samplerHeapMaxSize;
  }

VResourceHeap::Allocation VResourceHeap::alloc(const Sampler& s) {
  auto ret = allocatorSmp.alloc(1, [&](auto alloc) {
    auto vkWriteSamplerDescriptorsEXT = providerSmp.dev->vkWriteSamplerDescriptorsEXT;

    auto hPtrR = providerSmp.memory ? providerSmp.memory.handler->hptr : nullptr;
    auto res   = hPtrR + alloc.ptr*providerRes.elementSize;

    VkSamplerCreateInfo   info = VSamplerHeap::createInfo(*providerSmp.dev, s);
    VkHostAddressRangeEXT dest = {res, providerSmp.elementSize};
    vkAssert(vkWriteSamplerDescriptorsEXT(providerSmp.dev->device.impl, 1, &info, &dest));
    });
  return Allocation(ret.ptr);
  }

VResourceHeap::Allocation VResourceHeap::alloc(AbstractGraphicsApi::Buffer** buf, size_t cnt) {
  auto ret = allocatorRes.alloc(uint32_t(cnt), [&](auto alloc) {
    auto  dPtrR = alloc.ptr;
    auto  hPtrR = providerRes.memory ? providerRes.memory.handler->hptr : nullptr;

    for(size_t i=0; i<cnt; ++i) {
      auto res = hPtrR + (dPtrR + i)*providerRes.elementSize;
      VPushDescriptor::write(*providerRes.dev, res, ShaderReflection::SsboR, buf[i], 0, ComponentMapping());
      }
    });

  return Allocation(ret.ptr);
  }

VResourceHeap::Allocation VResourceHeap::alloc(AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel) {
  auto ret = allocatorRes.alloc(uint32_t(cnt), [&](auto alloc) {
    auto  dPtrR = alloc.ptr;
    auto  hPtrR = providerRes.memory ? providerRes.memory.handler->hptr : nullptr;

    for(size_t i=0; i<cnt; ++i) {
      auto res = hPtrR + (dPtrR + i)*providerRes.elementSize;
      VPushDescriptor::write(*providerRes.dev, res, ShaderReflection::Image, tex[i], mipLevel, ComponentMapping());
      }
    });

  return Allocation(ret.ptr);
  }

VResourceHeap::Allocation VResourceHeap::alloc(uint32_t num) {
  auto ret = allocatorRes.alloc(uint32_t(num));
  return Allocation(ret.ptr);
  }

void VResourceHeap::flush() {
  if(providerRes.dev==nullptr)
    return;
  allocatorRes.flush();
  allocatorSmp.flush();
  }

DSharedPtr<VDescriptorHeap*> VResourceHeap::currentMemory() const {
  std::lock_guard<SpinLock> guard(providerRes.sync);
  return providerRes.memory;
  }

auto VResourceHeap::currentMemorySmp() const -> DSharedPtr<VDescriptorHeap*> {
  std::lock_guard<SpinLock> guard(providerSmp.sync);
  return providerSmp.memory;
  }

void VResourceHeap::free(uint32_t ptr, uint32_t num) {
  allocatorRes.free(ptr, num);
  return;
  }

#endif