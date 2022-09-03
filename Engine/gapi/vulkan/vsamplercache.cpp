#if defined(TEMPEST_BUILD_VULKAN)

#include "vsamplercache.h"

#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VSamplerCache::VSamplerCache(){
  }

VSamplerCache::~VSamplerCache() {
  if(smpDefault!=VK_NULL_HANDLE)
    vkDestroySampler(device,smpDefault,nullptr);
  for(auto& i:chunks)
    vkDestroySampler(device,i.sampler,nullptr);
  }

void VSamplerCache::setDevice(VDevice &dev) {
  device        = dev.device.impl;
  anisotropy    = dev.props.anisotropy;
  maxAnisotropy = dev.props.maxAnisotropy;

  smpDefault    = alloc(Sampler());
  }

VkSampler VSamplerCache::get(Sampler s) {
  s.mapping = ComponentMapping();

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

VkSampler VSamplerCache::alloc(const Sampler &s) {
  VkSampler           sampler=VK_NULL_HANDLE;
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
  if(s.anisotropic && anisotropy) {
    samplerInfo.anisotropyEnable      = VK_TRUE;
    samplerInfo.maxAnisotropy         = maxAnisotropy;
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

  vkAssert(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));
  return sampler;
  }

#endif
