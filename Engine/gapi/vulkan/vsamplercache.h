#pragma once

#include <Tempest/Texture2d>
#include <mutex>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VSamplerCache {
  public:
    VSamplerCache();
    ~VSamplerCache();

    VkSampler get(uint32_t mipCount);
    VkSampler get(const Texture2d::Sampler& s, uint32_t mipCount);
    void      free(VkSampler s);
    void      freeLast();

    VkDevice           device =nullptr;

  private:
    struct Id {
      Texture2d::Sampler smp;
      VkSampler          sampler=VK_NULL_HANDLE;
      };

    struct Chunk {
      Chunk(uint32_t mipCount):mipCount(mipCount){}
      uint32_t        mipCount;
      std::vector<Id> samp;
      };

    std::mutex         sync;
    std::vector<Chunk> chunks;

    Chunk&             chunk(uint32_t mipCount);
    Id                 alloc(Chunk& c,const Texture2d::Sampler& s);
    VkSampler          alloc(const Texture2d::Sampler& s, uint32_t mipCount);
  };

}}
