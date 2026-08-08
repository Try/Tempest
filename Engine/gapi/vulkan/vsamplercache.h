#pragma once

#include <Tempest/Texture2d>
#include <mutex>
#include <vector>

#include "vulkan_sdk.h"
#include "utility/spinlock.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VSamplerCache final {
  public:
    VSamplerCache();
    ~VSamplerCache();

    static VkSamplerCreateInfo createInfo(const VDevice& dev, const Sampler& s);

    VkSampler get(const Sampler& s);
    void      setDevice(VDevice &dev);

  private:
    struct Entry {
      Sampler   smp;
      VkSampler sampler=VK_NULL_HANDLE;
      };

    SpinLock           sync;
    std::vector<Entry> chunks;

    VDevice*           device     = nullptr;
    VkSampler          smpDefault = VK_NULL_HANDLE;

    VkSampler          alloc(const Sampler& s);
  };

}}
