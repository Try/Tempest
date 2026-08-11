#pragma once

#include "vulkan_sdk.h"

#include "vbuffer.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VDescriptorHeap {
  public:
    VDescriptorHeap();
    ~VDescriptorHeap();

    void setDevice(VDevice& dev);

    struct Allocation {
      uint32_t                 ptr  = 0;
      uint8_t*                 hptr = nullptr;
      std::shared_ptr<VBuffer> memory;
      };

    Allocation alloc(AbstractGraphicsApi::Buffer**  buf, size_t cnt);
    Allocation alloc(AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel);
    Allocation alloc(uint32_t num);
    void       free (uint32_t ptr, uint32_t num);
    void       flush();

  private:
    struct VHeap;

    struct Range {
      uint32_t begin = 0;
      uint32_t end   = 0;
      };

    VDevice*  dev = nullptr;

    std::recursive_mutex   sync;
    std::shared_ptr<VHeap> memory;
    std::vector<Range>     rgn;
  };

}
}