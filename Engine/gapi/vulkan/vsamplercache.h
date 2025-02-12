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

    VkSampler get(const Sampler& s);
    void      setDevice(VDevice &dev);

  private:
    struct Entry {
      Sampler   smp;
      VkSampler sampler=VK_NULL_HANDLE;
      };

    SpinLock           sync;
    std::vector<Entry> chunks;

    VkDevice           device        = nullptr;
    VkSampler          smpDefault    = VK_NULL_HANDLE;

    bool               anisotropy    = false;
    float              maxAnisotropy = 1.f;

    VkSampler          alloc(const Sampler& s);
  };

}}
