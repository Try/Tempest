#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "dxuniformslay.h"

namespace Tempest {
namespace Detail {

class VUniformsLay;

class DxDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    DxDescriptorArray(DxDevice& dev, DxUniformsLay& vlay);
    ~DxDescriptorArray();

    void set    (size_t id, AbstractGraphicsApi::Texture *tex, const Sampler2d& smp) override;
    void setUbo (size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset, size_t size, size_t align) override;
    void setSsbo(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset, size_t size, size_t align) override;

    DSharedPtr<DxUniformsLay*>    layPtr;
    DxUniformsLay::PoolAllocation val;
    UINT                          heapCnt = 0;

  private:
    DxDevice&                     dev;
  };

}}
