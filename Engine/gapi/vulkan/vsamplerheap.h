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
    void      setDevice(VDevice &dev);

    uint32_t  getH(const Sampler& s);
    uint32_t  getH(const Sampler& s, const VTexture* tex);
    auto      currentMemory() const -> DSharedPtr<VDescriptorHeap*>;
    void      flush();

  private:
    struct Entry {
      Sampler   smp;
      VkSampler sampler=VK_NULL_HANDLE;
      };

    VkSampler alloc(const Sampler& s);
    uint32_t  allocHeap(const Sampler& s);
    void      alloc(void* pheap, const Sampler& s);

    mutable SpinLock   sync;
    std::vector<Entry> chunks;

    VDevice*           device     = nullptr;
    VkSampler          smpDefault = VK_NULL_HANDLE;

    DSharedPtr<VDescriptorHeap*> memory;
    std::vector<Sampler>         heapChunks;
  };

}}
