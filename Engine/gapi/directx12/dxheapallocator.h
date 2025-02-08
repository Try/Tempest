#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {
namespace Detail {

class DxDevice;

class DxHeapAllocator {
  public:
    DxHeapAllocator() = default;
    DxHeapAllocator(DxDevice& device);

    uint32_t alloc(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t num);
    void     free (D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t ptr, uint32_t num);

    UINT resSize = 0;
    UINT smpSize = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE res = {};
    D3D12_CPU_DESCRIPTOR_HANDLE smp = {};

    D3D12_GPU_DESCRIPTOR_HANDLE gres = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gsmp = {};

    ComPtr<ID3D12DescriptorHeap> resHeap;
    ComPtr<ID3D12DescriptorHeap> smpHeap;

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

    DxDevice* dev = nullptr;
    Allocator allocator[2];
  };

}
}
