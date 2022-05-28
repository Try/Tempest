#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "utility/smallarray.h"
#include "gapi/resourcestate.h"
#include "dxpipelinelay.h"

namespace Tempest {
namespace Detail {

class VPipelineLay;

class DxDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    DxDescriptorArray(DxPipelineLay& vlay);
    DxDescriptorArray(DxDescriptorArray&& other);
    ~DxDescriptorArray();

    void set    (size_t id, AbstractGraphicsApi::Texture *tex, const Sampler2d& smp, uint32_t mipLevel) override;
    void set    (size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset) override;
    void setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) override;
    void ssboBarriers(Detail::ResourceState& res) override;

    DSharedPtr<DxPipelineLay*>    lay;
    DxPipelineLay::PoolAllocation val;
    UINT                          heapCnt = 0;

  private:
    struct UAV {
      AbstractGraphicsApi::Texture* tex = nullptr;
      AbstractGraphicsApi::Buffer*  buf = nullptr;
      };
    SmallArray<UAV,16>            uav;
    ResourceState::Usage          uavUsage;
  };

}}
