#if defined(TEMPEST_BUILD_VULKAN)

#include "vsamplercache.h"

#include "vdevice.h"
#include "vtexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

VSamplerCache::VSamplerCache(){
  }

VSamplerCache::~VSamplerCache() {
  if(device->props.hasDescriptorHeap)
    return; // heap-allocator will clear whole vkBuffer

  if(smpDefault!=VK_NULL_HANDLE)
    vkDestroySampler(device->device.impl,smpDefault,nullptr);
  for(auto& i:chunks)
    vkDestroySampler(device->device.impl,i.sampler,nullptr);
  }

VkSamplerCreateInfo VSamplerCache::createInfo(const VDevice& dev, const Sampler& s) {
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

void VSamplerCache::setDevice(VDevice &dev) {
  device = &dev;

  if(device->props.hasDescriptorHeap) {
    smpDefault = device->descAlloc.alloc(Sampler()).ptr;
    } else {
    smpDefault = alloc(Sampler());
    }
  }

uint64_t VSamplerCache::implGet(const Sampler& s) {
  static const Sampler def;
  if(def==s)
    return smpDefault;

  std::lock_guard<SpinLock> guard(sync);
  for(auto& i:chunks) {
    if(i.smp==s)
      return i.value;
    }

  chunks.emplace_back();
  auto& b = chunks.back();
  b.smp = s;
  try {
    if(device->props.hasDescriptorHeap) {
      b.value = device->descAlloc.alloc(s).ptr;
      } else {
      b.value = alloc(s);
      }
    }
  catch(...) {
    chunks.pop_back();
    }
  return b.value;
  }

VkSampler VSamplerCache::get(const Sampler& s) {
  return VkSampler(implGet(s));
  }

VkSampler VSamplerCache::get(const Sampler& s, const VTexture* tex) {
  auto sx = s;
  if(!tex->isFilterable) {
    sx.setFiltration(Filter::Nearest);
    sx.anisotropic = false;
    }
  return get(sx);
  }

uint32_t VSamplerCache::getH(const Sampler& s) {
  return uint32_t(implGet(s));
  }

uint32_t VSamplerCache::getH(const Sampler& s, const VTexture* tex) {
  auto sx = s;
  if(!tex->isFilterable) {
    sx.setFiltration(Filter::Nearest);
    sx.anisotropic = false;
    }
  return getH(sx);
  }

VkSampler VSamplerCache::alloc(const Sampler &s) {
  VkSamplerCreateInfo info = createInfo(*device, s);

  VkSampler sampler = VK_NULL_HANDLE;
  vkAssert(vkCreateSampler(device->device.impl, &info, nullptr, &sampler));
  return sampler;
  }

#endif
