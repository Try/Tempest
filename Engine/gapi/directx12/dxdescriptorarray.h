#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "dxuniformslay.h"

namespace Tempest {
namespace Detail {

class VUniformsLay;

class DxDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    DxDescriptorArray(DxUniformsLay& vlay);
    DxDescriptorArray(DxDescriptorArray&& other);
    ~DxDescriptorArray();

    void set    (size_t id, AbstractGraphicsApi::Texture *tex, uint32_t mipLevel, const Sampler2d& smp);

    void set    (size_t id, AbstractGraphicsApi::Texture *tex, const Sampler2d& smp) override;
    void setUbo (size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset) override;
    void setSsbo(size_t id, AbstractGraphicsApi::Texture *tex, uint32_t mipLevel) override;
    void setSsbo(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offsetn) override;
    void ssboBarriers(Detail::ResourceState& res) override;

    DSharedPtr<DxUniformsLay*>    lay;
    DxUniformsLay::PoolAllocation val;
    UINT                          heapCnt = 0;
  };

}}
