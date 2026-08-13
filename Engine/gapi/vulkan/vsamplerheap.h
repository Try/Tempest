#pragma once

#include <Tempest/Texture2d>
#include <vector>

#include "vulkan_sdk.h"
#include "utility/spinlock.h"

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
    void      setDevice(VDevice &dev);

    uint32_t  getH(const Sampler& s);
    auto      getHeap() const -> std::shared_ptr<VBuffer>;
    void      bindHeap(VkCommandBuffer cmd, const VBuffer& buf);
    void      flush();

  private:
    struct Entry {
      Sampler   smp;
      VkSampler sampler=VK_NULL_HANDLE;
      };

    struct VHeap;

    VkSampler alloc(const Sampler& s);
    void      setupHeap();

    SpinLock           sync;
    std::vector<Entry> chunks;

    VDevice*           device     = nullptr;
    VkSampler          smpDefault = VK_NULL_HANDLE;

    std::shared_ptr<VHeap> samplersHeap;
    std::vector<Sampler>   heapChunks;
  };

}}
