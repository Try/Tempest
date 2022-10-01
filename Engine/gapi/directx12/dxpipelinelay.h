#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>
#include <vector>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"
#include "gapi/shaderreflection.h"

#include <bitset>

namespace Tempest {

class PipelineLayout;

namespace Detail {

class DxDescriptorArray;
class DxDevice;

class DxPipelineLay : public AbstractGraphicsApi::PipelineLay {
  public:
    DxPipelineLay(DxDevice& device, const std::vector<ShaderReflection::Binding>* sh);
    DxPipelineLay(DxDevice& device, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt,
                  bool has_baseVertex_baseInstance);

    size_t descriptorsCount() override;

    using Binding = ShaderReflection::Binding;

    enum {
      POOL_SIZE    = 128,
      MAX_BINDS    = 3,
      MAX_BINDLESS = 2048,
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
      DescriptorPool(DxPipelineLay& lay, uint32_t poolSize);
      DescriptorPool(DescriptorPool&& oth);
      ~DescriptorPool();

      ID3D12DescriptorHeap*     heap[MAX_BINDS] = {};
      std::bitset<POOL_SIZE>    allocated;
      };

    struct PoolAllocation {
      D3D12_CPU_DESCRIPTOR_HANDLE cpu [MAX_BINDS] = {};
      D3D12_GPU_DESCRIPTOR_HANDLE gpu [MAX_BINDS] = {};
      ID3D12DescriptorHeap*       heap[MAX_BINDS] = {};
      size_t                      pool           = 0;
      size_t                      offset         = 0;
      };

    std::vector<Param>          prm;
    std::vector<Heap>           heaps;
    std::vector<RootPrm>        roots;
    uint32_t                    pushConstantId     = uint32_t(-1);
    uint32_t                    pushBaseInstanceId = uint32_t(-1);
    std::vector<Binding>        lay;

    ComPtr<ID3D12RootSignature> impl;
    DxDevice&                   dev;

    PoolAllocation              allocDescriptors();
    void                        freeDescriptors(PoolAllocation& d);

  private:
    struct Parameter final {
      D3D12_DESCRIPTOR_RANGE  rgn;
      uint32_t                id=0;
      D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
      };

    SpinLock                    poolsSync;
    std::vector<DescriptorPool> pools;

    UINT                        descSize = 0;
    UINT                        smpSize  = 0;
    bool                        runtimeSized = false;

    uint32_t findBinding(const std::vector<D3D12_ROOT_PARAMETER>& except) const;

    void init(const std::vector<Binding>& lay, const ShaderReflection::PushBlock& pb,
              uint32_t runtimeArraySz, bool has_baseVertex_baseInstance);
    void add (const ShaderReflection::Binding& b, D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t cnt, std::vector<Parameter>& root);
    void adjustSsboBindings();
  };

}
}

