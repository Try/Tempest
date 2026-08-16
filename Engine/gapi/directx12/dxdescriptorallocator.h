#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>

#include "gapi/descriptorallocator.h"
#include "gapi/deviceallocator.h"

#include "comptr.h"

namespace Tempest {
namespace Detail {

class DxDevice;

class DxDescriptorAllocator {
  public:
    DxDescriptorAllocator();

    struct Provider {
      using DeviceMemory = ID3D12DescriptorHeap*;
      ~Provider();

      DxDevice*    device     = nullptr;
      DeviceMemory last       = nullptr;
      size_t       lastSize   = 0;
      uint32_t     lastTypeId = 0;

      DeviceMemory alloc(size_t size, uint32_t typeId);
      void         free(DeviceMemory m, size_t size, uint32_t typeId);
      };

    struct ProviderHeap {
      DxDevice*                    dev = nullptr;
      ComPtr<ID3D12DescriptorHeap> memory;
      D3D12_CPU_DESCRIPTOR_HANDLE  cpu     = {};
      D3D12_GPU_DESCRIPTOR_HANDLE  gpu     = {};
      uint32_t                     memSize = 0;

      D3D12_DESCRIPTOR_HEAP_TYPE   type        = {};
      uint32_t                     elementSize = 0;
      uint32_t                     reserveSize = 0;
      uint32_t                     maxSize     = 0;

      uint32_t size() const { return memSize; }
      void     realloc(uint32_t size);
      void     flush(){}
      };

    void       setDevice(DxDevice& device);

    using Allocation = typename Tempest::Detail::DeviceAllocator<Provider>::Allocation;

    Allocation allocHost(size_t count);
    Allocation allocRtv (size_t count);
    Allocation allocDsv (size_t count);
    uint32_t   allocRes (uint32_t num);
    uint32_t   allocSmp (uint32_t num);
    uint32_t   alloc(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t num);
    void       free (Allocation& page);
    void       free (D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t ptr, uint32_t num);

    D3D12_CPU_DESCRIPTOR_HANDLE handleCpu(const Allocation& a, size_t offset = 0);
    D3D12_CPU_DESCRIPTOR_HANDLE handleCpu(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t offset);
    D3D12_GPU_DESCRIPTOR_HANDLE handleGpu(D3D12_DESCRIPTOR_HEAP_TYPE heap, uint32_t offset);

    ID3D12DescriptorHeap*       currentMemory(D3D12_DESCRIPTOR_HEAP_TYPE heap) const;

    uint32_t                          resSize = 1;
    uint32_t                          smpSize = 1;

  private:
    Provider                          providerHost;
    Detail::DeviceAllocator<Provider> allocatorHost{providerHost};

    ProviderHeap                      providerRes;
    ProviderHeap                      providerSmp;
    DescriptorAllocator<ProviderHeap> allocatorRes{providerRes};
    DescriptorAllocator<ProviderHeap> allocatorSmp{providerSmp};

    uint32_t                          rtvSize = 1;
    uint32_t                          dsvSize = 1;
  };

}
}


