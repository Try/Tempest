#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "gapi/resourcestate.h"
#include "utility/smallarray.h"
#include "dxdescriptorallocator.h"
#include "dxpipelinelay.h"

namespace Tempest {
namespace Detail {

class DxTexture;
class DxBuffer;
class DxAccelerationStructure;

class DxDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    DxDescriptorArray(DxPipelineLay& vlay);
    DxDescriptorArray(DxDescriptorArray&& other);
    ~DxDescriptorArray();

    void set    (size_t id, AbstractGraphicsApi::Texture *tex, const Sampler& smp, uint32_t mipLevel) override;
    void set    (size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset) override;
    void set    (size_t id, const Sampler& smp) override;
    void setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) override;

    void set    (size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt, const Sampler& smp, uint32_t mipLevel) override;
    void set    (size_t id, AbstractGraphicsApi::Buffer**  buf, size_t cnt) override;

    void ssboBarriers(Detail::ResourceState& res, PipelineStage st) override;

    void bindRuntimeSized(ID3D12GraphicsCommandList6& enc, ID3D12DescriptorHeap** currentHeaps, bool isCompute);

    DSharedPtr<DxPipelineLay*>         lay;
    DxPipelineLay::PoolAllocation      val;
    UINT                               heapCnt = 0;

  private:
    enum {
      HEAP_RES = DxPipelineLay::HEAP_RES,
      HEAP_SMP = DxPipelineLay::HEAP_SMP,
      HEAP_MAX = DxPipelineLay::HEAP_MAX,
      };
    using Allocation = DxDescriptorAllocator::Allocation;

    void reallocSet(size_t id, uint32_t newRuntimeSz);
    void reflushSet();

    void placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn, const D3D12_CPU_DESCRIPTOR_HANDLE& at,
                     UINT64 heapOffset, DxTexture& t, const ComponentMapping& map, uint32_t mipLevel);
    void placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn, const D3D12_CPU_DESCRIPTOR_HANDLE& at,
                     UINT64 heapOffset, const Sampler& smp);
    void placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn, const D3D12_CPU_DESCRIPTOR_HANDLE& at,
                     UINT64 heapOffset, DxBuffer& buf, uint64_t bufOffset, uint64_t byteSize);
    void placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn, const D3D12_CPU_DESCRIPTOR_HANDLE& at,
                     UINT64 heapOffset, DxAccelerationStructure& as);

    struct UAV {
      AbstractGraphicsApi::Texture* tex = nullptr;
      AbstractGraphicsApi::Buffer*  buf = nullptr;
      };
    SmallArray<UAV,16>            uav;
    ResourceState::Usage          uavUsage;

    struct DynBinding {
      size_t                      heapOffset    = 0;
      size_t                      heapOffsetSmp = 0;
      size_t                      size          = 0;

      std::vector<AbstractGraphicsApi::Shared*> data;
      Sampler                                   smp;
      uint32_t                                  mipLevel = 0;
      size_t                                    offset   = 0;
      };
    Allocation                    heapDyn[DxPipelineLay::HEAP_MAX] = {};
    std::vector<DynBinding>       runtimeArrays;
  };

}}
