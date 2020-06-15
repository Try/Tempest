#include "vsamplercache.h"

#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VSamplerCache::VSamplerCache(){
  }

VSamplerCache::~VSamplerCache() {
  }

VkSampler VSamplerCache::get(uint32_t mipCount) {
  return get(Tempest::Sampler2d(),mipCount);
  }

VkSampler VSamplerCache::get(const Sampler2d &s, uint32_t mipCount) {
  std::lock_guard<std::mutex> guard(sync);

  auto& c = chunk(mipCount);
  return alloc(c,s).sampler;
  }

void VSamplerCache::free(VkSampler ) {
  }

void VSamplerCache::freeLast() {
  vkDeviceWaitIdle(device);
  for(auto& c:chunks)
    for(auto& i:c.samp)
      vkDestroySampler(device,i.sampler,nullptr);
  chunks.clear();
  }

void VSamplerCache::setDevice(VDevice &dev) {
  device        = dev.device;
  anisotropy    = dev.props.anisotropy;
  maxAnisotropy = dev.props.maxAnisotropy;
  }

VSamplerCache::Chunk &VSamplerCache::chunk(uint32_t mipCount) {
  for(auto& i:chunks)
    if(i.mipCount==mipCount)
      return i;
  chunks.emplace_back(mipCount);
  return chunks.back();
  }

VSamplerCache::Id VSamplerCache::alloc(VSamplerCache::Chunk &c, const Sampler2d &s) {
  for(auto& i:c.samp)
    if(i.smp==s)
      return i;
  Id id;
  id.smp     = s;
  id.sampler = alloc(s,c.mipCount);

  c.samp.emplace_back(id);
  return c.samp.back();
  }

VkSampler VSamplerCache::alloc(const Sampler2d &s, uint32_t mipCount) {
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
  samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  if(s.anisotropic && anisotropy){
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = maxAnisotropy;
    } else {
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.maxAnisotropy           = 1.f;
    }
  samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable           = VK_FALSE;
  samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;

  samplerInfo.minLod                  = 0;
  samplerInfo.maxLod                  = static_cast<float>(mipCount);

  vkAssert(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));
  return sampler;
  }
