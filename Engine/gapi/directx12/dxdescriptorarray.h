#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "dxpipelinelay.h"

namespace Tempest {
namespace Detail {

class VPipelineLay;

class DxDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    DxDescriptorArray(DxPipelineLay& vlay);
    DxDescriptorArray(DxDescriptorArray&& other);
    ~DxDescriptorArray();

    void set    (size_t id, const AbstractGraphicsApi::Texture* tex, uint32_t mipLevel, const Sampler2d& smp, DXGI_FORMAT vfrm);

    void set    (size_t id, AbstractGraphicsApi::Texture *tex, const Sampler2d& smp) override;
    void setUbo (size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset) override;
    void setSsbo(size_t id, AbstractGraphicsApi::Texture *tex, uint32_t mipLevel) override;
    void setSsbo(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offsetn) override;
    void ssboBarriers(Detail::ResourceState& res) override;

    DSharedPtr<DxPipelineLay*>    lay;
    DxPipelineLay::PoolAllocation val;
    UINT                          heapCnt = 0;

  private:
    struct SSBO {
      AbstractGraphicsApi::Texture* tex = nullptr;
      AbstractGraphicsApi::Buffer*  buf = nullptr;
      };
    std::unique_ptr<SSBO[]>  ssbo;
  };

}}
