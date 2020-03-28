#pragma once

#include <Tempest/Texture2d>
#include <mutex>
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VSamplerCache final {
  public:
    VSamplerCache();
    ~VSamplerCache();

    VkSampler get(uint32_t mipCount);
    VkSampler get(const Sampler2d& s, uint32_t mipCount);
    void      free(VkSampler s);
    void      freeLast();

    void      setDevice(VDevice &dev);

  private:
    struct Id {
      Sampler2d smp;
      VkSampler sampler=VK_NULL_HANDLE;
      };

    struct Chunk {
      Chunk(uint32_t mipCount):mipCount(mipCount){}
      uint32_t        mipCount;
      std::vector<Id> samp;
      };

    std::mutex         sync;
    std::vector<Chunk> chunks;

    VkDevice           device    =nullptr;
    bool               anisotropy=false;
    float              maxAnisotropy=1.f;

    Chunk&             chunk(uint32_t mipCount);
    Id                 alloc(Chunk& c,const Sampler2d& s);
    VkSampler          alloc(const Sampler2d& s, uint32_t mipCount);
  };

}}
