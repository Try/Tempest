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

    enum {
      HEAP_RES          = DxPipelineLay::HEAP_RES,
      HEAP_SMP          = DxPipelineLay::HEAP_SMP,
      HEAP_MAX          = DxPipelineLay::HEAP_MAX,
      ALLOC_GRANULARITY = 4,
      };

    struct CbState {
      ID3D12DescriptorHeap* heaps[HEAP_MAX] = {};
      };

    void set(size_t id, AbstractGraphicsApi::Texture *tex, const Sampler& smp, uint32_t mipLevel) override;
    void set(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset) override;
    void set(size_t id, const Sampler& smp) override;
    void set(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) override;
    void set(size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt, const Sampler& smp, uint32_t mipLevel) override;
    void set(size_t id, AbstractGraphicsApi::Buffer**  buf, size_t cnt) override;

    void ssboBarriers(Detail::ResourceState& res, PipelineStage st) override;

    void bind(ID3D12GraphicsCommandList6& enc, CbState& state, bool isCompute);

    DSharedPtr<DxPipelineLay*> lay;

  private:
    using Allocation = DxDescriptorAllocator::Allocation;

    void reallocSet(size_t id, size_t newRuntimeSz);
    void reflushSet();

    void placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn, const D3D12_CPU_DESCRIPTOR_HANDLE& at,
                     UINT64 heapOffset, DxTexture& t, const ComponentMapping& map, uint32_t mipLevel);
    void placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn, const D3D12_CPU_DESCRIPTOR_HANDLE& at,
                     UINT64 heapOffset, const Sampler& smp);
    void placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn, const D3D12_CPU_DESCRIPTOR_HANDLE& at,
                     UINT64 heapOffset, DxBuffer* buf, uint64_t bufOffset, uint64_t byteSize);
    void placeInHeap(ID3D12Device& device, D3D12_DESCRIPTOR_RANGE_TYPE rgn, const D3D12_CPU_DESCRIPTOR_HANDLE& at,
                     UINT64 heapOffset, DxAccelerationStructure& as);

    struct UAV {
      AbstractGraphicsApi::Texture* tex = nullptr;
      AbstractGraphicsApi::Buffer*  buf = nullptr;
      };
    SmallArray<UAV,16>            uav;
    ResourceState::Usage          uavUsage;
    Allocation                    heap[DxPipelineLay::HEAP_MAX] = {};

    struct DynBinding {
      size_t                      heapOffset    = 0;
      size_t                      heapOffsetSmp = 0;
      size_t                      size          = 0;

      std::vector<AbstractGraphicsApi::Shared*> data;
      Sampler                                   smp;
      uint32_t                                  mipLevel = 0;
      size_t                                    offset   = 0;
      };
    std::vector<DynBinding>       runtimeArrays;
  };

class DxDescriptorArray2 : public AbstractGraphicsApi::DescArray {
  public:
    DxDescriptorArray2(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler& smp);
    DxDescriptorArray2(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel);
    DxDescriptorArray2(DxDevice& dev, AbstractGraphicsApi::Buffer**  buf, size_t cnt);
    ~DxDescriptorArray2();

    size_t size() const;
    auto   handleR()  const -> D3D12_GPU_DESCRIPTOR_HANDLE;
    auto   handleRW() const -> D3D12_GPU_DESCRIPTOR_HANDLE;

  private:
    void   clear();

    DxDevice& dev;
    uint32_t  dPtrR  = 0xFFFFFFFF;
    uint32_t  dPtrRW = 0xFFFFFFFF;
    size_t    cnt   = 0;
  };
}}
