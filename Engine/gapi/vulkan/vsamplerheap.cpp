#if defined(TEMPEST_BUILD_VULKAN)

#include "vsamplerheap.h"

#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

struct VSamplerHeap::VHeap : VBuffer {
  VHeap(VBuffer&& v):VBuffer(std::move(v)) {
    hptr = this->mapDescriptorHeap();
    }
  ~VHeap() {
    unmapDescriptorHeap();
    }
  uint8_t* hptr = nullptr;
  };

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
    setupHeap();
    } else {
    smpDefault = alloc(Sampler());
    }
  }

std::shared_ptr<VBuffer> VSamplerHeap::currentMemory() const {
  return samplersHeap;
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
  if(def==s)
    return 0;

  std::lock_guard<SpinLock> guard(sync);
  for(size_t i=0; i<heapChunks.size(); ++i) {
    if(heapChunks[i]==s)
      return uint32_t(i);
    }

  auto maxSamplers = (samplersHeap->size() - device->props.samplerHeapReserve)/device->props.samplerDescriptorSize;
  if(heapChunks.size()>=maxSamplers)
    throw std::bad_alloc();

  auto vkWriteSamplerDescriptorsEXT = device->vkWriteSamplerDescriptorsEXT;

  const auto ptr = samplersHeap->hptr + heapChunks.size()*device->props.samplerDescriptorSize;
  VkSamplerCreateInfo   info = VSamplerHeap::createInfo(*device, s);
  VkHostAddressRangeEXT dest = {ptr, device->props.samplerDescriptorSize};
  vkAssert(vkWriteSamplerDescriptorsEXT(device->device.impl, 1, &info, &dest));

  heapChunks.emplace_back(s);
  return uint32_t(heapChunks.size()-1);
  }

void VSamplerHeap::bindHeap(VkCommandBuffer cmd, const VBuffer& buf) {
  if(samplersHeap==nullptr || samplersHeap->hptr==nullptr)
    return;

  //NOTE: might be a problem with bindless-only draws
  auto& samplers = *samplersHeap;

  auto vkCmdBindSamplerHeapEXT  = device->vkCmdBindSamplerHeapEXT;

  VkBindHeapInfoEXT info = {VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
  info.heapRange.address   = samplers.toDeviceAddress(*device);
  info.heapRange.size      = samplers.size();
  info.reservedRangeOffset = info.heapRange.size - device->props.samplerHeapReserve;
  info.reservedRangeSize   = device->props.samplerHeapReserve;
  vkCmdBindSamplerHeapEXT(cmd, &info);
  }

void VSamplerHeap::flush() {
  if(samplersHeap!=nullptr)
    device->allocator.flushDescriptorHeap(*samplersHeap);
  }

VkSampler VSamplerHeap::alloc(const Sampler &s) {
  VkSampler           sampler = VK_NULL_HANDLE;
  VkSamplerCreateInfo info    = createInfo(*device, s);

  vkAssert(vkCreateSampler(device->device.impl, &info, nullptr, &sampler));
  return sampler;
  }

void VSamplerHeap::setupHeap() {
  auto vkWriteSamplerDescriptorsEXT = device->vkWriteSamplerDescriptorsEXT;

  const auto& props   = device->props;
  const auto  maxSize = props.samplerHeapMaxSize;

  auto buf     = device->allocator.alloc(nullptr, maxSize, MemUsage::Descriptor, BufferHeap::Upload);
  samplersHeap = std::make_shared<VHeap>(std::move(buf));

  VkSamplerCreateInfo   info = VSamplerHeap::createInfo(*device, Sampler());
  VkHostAddressRangeEXT dest = {samplersHeap->hptr, props.samplerDescriptorSize};
  vkAssert(vkWriteSamplerDescriptorsEXT(device->device.impl, 1, &info, &dest));

  heapChunks.push_back(Sampler());
  }

#endif
