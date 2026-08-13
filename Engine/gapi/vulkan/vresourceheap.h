#pragma once

#include "vulkan_sdk.h"

#include "vbuffer.h"

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
    void       free (uint32_t ptr, uint32_t num);
    void       flush();

    std::shared_ptr<VDescriptorHeap> currentMemory() const;

  private:

    struct Range {
      uint32_t begin = 0;
      uint32_t end   = 0;
      };

    VDevice*  dev = nullptr;

    mutable std::recursive_mutex     sync;
    std::shared_ptr<VDescriptorHeap> memory;
    std::vector<Range>     rgn;
  };

}
}