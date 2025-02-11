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

class DxDescriptorArray {
  public:
    enum {
      HEAP_RES          = DxPipelineLay::HEAP_RES,
      HEAP_SMP          = DxPipelineLay::HEAP_SMP,
      HEAP_MAX          = DxPipelineLay::HEAP_MAX,
      ALLOC_GRANULARITY = 4,
      };

    struct CbState {
      ID3D12DescriptorHeap* heaps[HEAP_MAX] = {};
      };
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
