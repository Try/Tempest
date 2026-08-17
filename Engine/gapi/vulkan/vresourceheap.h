#pragma once

#include "vulkan_sdk.h"

#include "gapi/descriptorallocator.h"
#include "gapi/vulkan/vbuffer.h"
#include "utility/spinlock.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VResourceHeap {
  public:
    VResourceHeap();
    ~VResourceHeap();

    void setDevice(VDevice& dev);

    struct Allocation {
      uint32_t ptr = 0;
      };

    Allocation alloc(AbstractGraphicsApi::Buffer**  buf, size_t cnt);
    Allocation alloc(AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel);
    Allocation alloc(uint32_t num);
    Allocation alloc(const Sampler& s);
    void       free (uint32_t ptr, uint32_t num);
    void       flush();

    auto       currentMemory() const -> DSharedPtr<VDescriptorHeap*>;
    auto       currentMemorySmp() const -> DSharedPtr<VDescriptorHeap*>;

  private:
    struct Provider {
      VDevice*                     dev = nullptr;

      mutable SpinLock             sync;
      DSharedPtr<VDescriptorHeap*> memory;

      uint32_t                     elementSize = 0;
      uint32_t                     reserveSize = 0;
      uint32_t                     maxSize     = 0;

      uint32_t size() const { return memory.handler!=nullptr ? uint32_t(memory.handler->size()) : 0; }
      void     realloc(uint32_t size);
      void     flush();
      };

    Provider                      providerRes;
    DescriptorAllocator<Provider> allocatorRes {providerRes};

    Provider                      providerSmp;
    DescriptorAllocator<Provider> allocatorSmp {providerSmp};
  };

}
}