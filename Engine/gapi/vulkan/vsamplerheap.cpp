#if defined(TEMPEST_BUILD_VULKAN)

#include "vsamplerheap.h"

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


VSamplerHeap::VSamplerHeap(){
  }

VSamplerHeap::~VSamplerHeap() {
  if(smpDefault!=VK_NULL_HANDLE)
    vkDestroySampler(device->device.impl,smpDefault,nullptr);
  for(auto& i:chunks)
    vkDestroySampler(device->device.impl,i.sampler,nullptr);
  }

VkSamplerCreateInfo VSamplerHeap::createInfo(const VDevice& dev, const Sampler& s) {
  VkSamplerCreateInfo samplerInfo = {};
  samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

  samplerInfo.magFilter               = nativeFormat(s.magFilter);
  samplerInfo.minFilter               = nativeFormat(s.minFilter);
  if(s.mipFilter==Filter::Nearest)
    samplerInfo.mipmapMode            = VK_SAMPLER_MIPMAP_MODE_NEAREST; else
    samplerInfo.mipmapMode            = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.addressModeU            = nativeFormat(s.uClamp);
  samplerInfo.addressModeV            = nativeFormat(s.vClamp);
  samplerInfo.addressModeW            = nativeFormat(s.wClamp);
  if(s.anisotropic && dev.props.anisotropy) {
    samplerInfo.anisotropyEnable      = VK_TRUE;
    samplerInfo.maxAnisotropy         = dev.props.maxAnisotropy;
    } else {
    samplerInfo.anisotropyEnable      = VK_FALSE;
    samplerInfo.maxAnisotropy         = 1.f;
    }
  samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable           = VK_FALSE;
  samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;

  samplerInfo.minLod                  = 0;
  samplerInfo.maxLod                  = VK_LOD_CLAMP_NONE;

  return samplerInfo;
  }

void VSamplerHeap::setDevice(VDevice &dev) {
  device     = &dev;
  if(dev.props.hasDescriptorHeap) {
    allocHeap(Sampler()); // allocate default sampler
    } else {
    smpDefault = alloc(Sampler());
    }
  }

DSharedPtr<VDescriptorHeap*> VSamplerHeap::currentMemory() const {
  std::lock_guard<SpinLock> guard(sync);
  return memory;
  }

VkSampler VSamplerHeap::get(const Sampler& s) {
  static const Sampler def;
  if(def==s)
    return smpDefault;

  std::lock_guard<SpinLock> guard(sync);
  for(auto& i:chunks) {
    if(i.smp==s)
      return i.sampler;
    }

  chunks.emplace_back();
  auto& b = chunks.back();
  b.smp = s;
  try {
    b.sampler = alloc(s);
    }
  catch(...) {
    chunks.pop_back();
    }
  return b.sampler;
  }

uint32_t VSamplerHeap::getH(const Sampler& s) {
  static const Sampler def;
  if(def==s) {
    auto& props  = device->props;
    auto  elSize = props.samplerDescriptorSize;
    auto  offset = ((props.samplerHeapReserve + elSize - 1) / elSize);
    return offset;
    }
  return allocHeap(s);
  }

uint32_t VSamplerHeap::allocHeap(const Sampler& s) {
  auto& props    = device->props;
  auto  maxSize  = props.samplerHeapMaxSize;
  auto  elSize   = props.samplerDescriptorSize;
  auto  offset   = ((props.samplerHeapReserve + elSize - 1) / elSize);
  auto  reserve  = offset * elSize;

  std::lock_guard<SpinLock> guard(sync);
  for(size_t i=0; i<heapChunks.size(); ++i) {
    if(heapChunks[i]==s)
      return uint32_t(offset + i);
    }

  auto  dstSize  = reserve + heapChunks.size()*elSize + elSize;
  if(memory.handler!=nullptr && dstSize < memory.handler->size()) {
    const auto ptr = memory.handler->hptr + reserve + heapChunks.size()*elSize;
    alloc(ptr, s);
    heapChunks.emplace_back(s);
    return uint32_t(offset + heapChunks.size()-1);
    }

  //auto  prevSize = uint32_t(memory ? memory->size() : 0);
  auto  size     = uint32_t(memory ? memory.handler->size() : reserve) + elSize;
  if(size > maxSize)
    throw std::bad_alloc();

  size = std::min(std::max(nextPot(size), 4*1024u), maxSize);
  auto buf  = device->allocator.alloc(nullptr, size, MemUsage::Descriptor, BufferHeap::Upload);
  auto next = DSharedPtr<VDescriptorHeap*>(new VDescriptorHeap(std::move(buf)));

  if(memory) {
    memory.handler->flush();
    std::memcpy(next.handler->hptr + reserve, memory.handler->hptr + reserve, memory.handler->size() - reserve);
    }

  const auto ptr = next.handler->hptr + reserve + heapChunks.size()*elSize;
  alloc(ptr, s);
  heapChunks.emplace_back(s);

  memory = next;
  return uint32_t(offset + heapChunks.size()-1);
  }

void VSamplerHeap::flush() {
  std::lock_guard<SpinLock> guard(sync);
  if(memory.handler!=nullptr)
    memory.handler->flush();
  }

VkSampler VSamplerHeap::alloc(const Sampler &s) {
  VkSamplerCreateInfo info = createInfo(*device, s);

  VkSampler sampler = VK_NULL_HANDLE;
  vkAssert(vkCreateSampler(device->device.impl, &info, nullptr, &sampler));
  return sampler;
  }

void VSamplerHeap::alloc(void* ptr, const Sampler& s) {
  auto vkWriteSamplerDescriptorsEXT = device->vkWriteSamplerDescriptorsEXT;

  VkSamplerCreateInfo   info = VSamplerHeap::createInfo(*device, s);
  VkHostAddressRangeEXT dest = {ptr, device->props.samplerDescriptorSize};
  vkAssert(vkWriteSamplerDescriptorsEXT(device->device.impl, 1, &info, &dest));
  }

#endif
