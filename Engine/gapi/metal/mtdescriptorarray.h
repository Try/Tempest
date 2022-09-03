#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/Metal.hpp>
#include "utility/smallarray.h"

namespace Tempest {
namespace Detail {

class MtPipelineLay;
class MtDevice;

class MtDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    MtDescriptorArray(MtDevice& dev, const MtPipelineLay &lay);

    void set    (size_t id, AbstractGraphicsApi::Texture* tex, const Sampler& smp, uint32_t mipLevel) override;
    void set    (size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset) override;
    void set    (size_t id, const Sampler& smp) override;
    void setTlas(size_t,AbstractGraphicsApi::AccelerationStructure*) override;

    struct Desc {
      void*              val     = nullptr;
      MTL::SamplerState* sampler = nullptr;
      size_t             offset  = 0;
      };

    MtDevice&                        dev;
    SmallArray<Desc,16>              desc;
    DSharedPtr<const MtPipelineLay*> lay;
  };

}
}


