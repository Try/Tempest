#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/UniformsLayout>
#include <vector>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

#include <bitset>

namespace Tempest {

class UniformsLayout;

namespace Detail {

class DxDescriptorArray;
class DxDevice;

class DxUniformsLay : public AbstractGraphicsApi::UniformsLay {
  public:
    DxUniformsLay(DxDevice& device, const std::vector<UniformsLayout::Binding>& comp);
    DxUniformsLay(DxDevice& device, const std::vector<UniformsLayout::Binding>* sh[], size_t cnt);

    using Binding = UniformsLayout::Binding;

    enum {
      POOL_SIZE = 128,
      MAX_BINDS = 3
      };

    struct Param {
      UINT64  heapOffset=0;
      uint8_t heapId=0;

      UINT64  heapOffsetSmp=0;
      uint8_t heapIdSmp=0;

      D3D12_DESCRIPTOR_RANGE_TYPE rgnType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      };

    struct Heap {
      D3D12_DESCRIPTOR_HEAP_TYPE type    = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      UINT                       numDesc = 0;
      };

    struct RootPrm {
      size_t heap = 0;
      UINT   heapOffset = 0;
      };

    struct DescriptorPool {
      DescriptorPool(DxUniformsLay& lay);
      DescriptorPool(DescriptorPool&& oth);
      ~DescriptorPool();

      ID3D12DescriptorHeap*     heap[MAX_BINDS] = {};
      std::bitset<POOL_SIZE>    allocated;
      };

    struct PoolAllocation {
      D3D12_CPU_DESCRIPTOR_HANDLE cpu[MAX_BINDS] = {};
      D3D12_GPU_DESCRIPTOR_HANDLE gpu[MAX_BINDS] = {};
      ID3D12DescriptorHeap*       heap[MAX_BINDS] = {};
      size_t                      pool           = 0;
      size_t                      offset         = 0;
      };

    std::vector<Param>          prm;
    std::vector<Heap>           heaps;
    std::vector<RootPrm>        roots;
    size_t                      pushConstantId=0;

    ComPtr<ID3D12RootSignature> impl;

    PoolAllocation              allocDescriptors();
    void                        freeDescriptors(PoolAllocation& d);

  private:
    struct Parameter final {
      D3D12_DESCRIPTOR_RANGE  rgn;
      uint32_t                id=0;
      D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
      };

    DxDevice&                   dev;
    SpinLock                    poolsSync;
    std::vector<DescriptorPool> pools;

    UINT                        descSize = 0;
    UINT                        smpSize  = 0;

    void init(const std::vector<Binding>& lay, const UniformsLayout::PushBlock& pb);
    void add(const UniformsLayout::Binding& b, D3D12_DESCRIPTOR_RANGE_TYPE type, std::vector<Parameter>& root);
  };

}
}

