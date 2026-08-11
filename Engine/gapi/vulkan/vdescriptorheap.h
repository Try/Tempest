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

    struct VHeap : VBuffer {
      VHeap(VBuffer&&);
      ~VHeap();

      uint8_t* hptr = nullptr;
      };

    struct Allocation {
      uint32_t                 ptr  = 0;
      uint8_t*                 hptr = nullptr;
      std::shared_ptr<VBuffer> memory;
      };

    Allocation alloc(AbstractGraphicsApi::Buffer**  buf, size_t cnt);
    Allocation alloc(AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel);
    Allocation alloc(HeapType heap, uint32_t num);
    void       free (HeapType heap, uint32_t ptr, uint32_t num);
    void       flush();

  private:
    struct Range {
      uint32_t begin = 0;
      uint32_t end   = 0;
      };

    struct Allocator {
      std::recursive_mutex     sync;
      std::vector<Range>       rgn;
      std::shared_ptr<VBuffer> memory;
      uint8_t*                 ptr = nullptr;
      };

    Allocation alloc(Allocator& heap, HeapType heapType, uint32_t num);
    void       free (Allocator& heap, uint32_t ptr, uint32_t num);

    VDevice*  dev = nullptr;
    Allocator allocator[2];
  };

}
}