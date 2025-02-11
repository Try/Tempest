#pragma once

#include <Tempest/AbstractGraphicsApi>

//#include "dxdevice.h"
#include "dxpipelinelay.h"

namespace Tempest {
namespace Detail {

class DxTexture;
class DxBuffer;
class DxAccelerationStructure;

class DxDescriptorArray : public AbstractGraphicsApi::DescArray {
  public:
    DxDescriptorArray(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler* smp);
    DxDescriptorArray(DxDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel);
    DxDescriptorArray(DxDevice& dev, AbstractGraphicsApi::Buffer**  buf, size_t cnt);
    ~DxDescriptorArray();

    size_t size() const;
    auto   handleR()  const -> D3D12_GPU_DESCRIPTOR_HANDLE;
    auto   handleRW() const -> D3D12_GPU_DESCRIPTOR_HANDLE;
    auto   handleS()  const -> D3D12_GPU_DESCRIPTOR_HANDLE;

  private:
    void   clear();

    DxDevice& dev;
    uint32_t  dPtrR  = 0xFFFFFFFF;
    uint32_t  dPtrRW = 0xFFFFFFFF;
    uint32_t  dPtrS  = 0xFFFFFFFF;
    size_t    cnt   = 0;
  };
}}
