#pragma once

#include <Tempest/Texture2d>
#include <vector>

#include "gapi/vulkan/vbuffer.h"
#include "utility/spinlock.h"

#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VBuffer;

class VSamplerHeap final {
  public:
    VSamplerHeap();
    ~VSamplerHeap();

    static VkSamplerCreateInfo createInfo(const VDevice& dev, const Sampler& s);

    VkSampler get(const Sampler& s);
    VkSampler get(const Sampler& s, const VTexture* tex);
    uint32_t  getH(const Sampler& s);
    uint32_t  getH(const Sampler& s, const VTexture* tex);
    void      setDevice(VDevice &dev);

  private:
    struct Entry {
      Sampler   smp;
      VkSampler sampler = VK_NULL_HANDLE;
      uint64_t  value   = 0;
      };

    uint64_t  implGet(const Sampler& s);
    VkSampler alloc(const Sampler& s);

    mutable SpinLock   sync;
    std::vector<Entry> chunks;

    VDevice*           device     = nullptr;
    uint64_t           smpDefault = VK_NULL_HANDLE;
  };

}}
