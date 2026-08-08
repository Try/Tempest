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
      uint32_t ptr = 0;
      };

    uint32_t alloc(HeapType heap, uint32_t num);
    void     free (HeapType heap, uint32_t ptr, uint32_t num);

    //NOTE: temporarly public
    VBuffer resources;
    VBuffer samplers;

    uint8_t* resourcesPtr = nullptr;
    uint8_t* samplersPtr  = nullptr;

  private:
    struct Range {
      uint32_t begin = 0;
      uint32_t end   = 0;
      };

    struct Allocator {
      std::mutex         sync;
      std::vector<Range> rgn;
      };

    uint32_t alloc(Allocator& heap, uint32_t num);
    void     free (Allocator& heap, uint32_t ptr, uint32_t num);

    VDevice*  dev = nullptr;
    Allocator allocator[2];
  };

}
}