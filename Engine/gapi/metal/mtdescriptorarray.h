#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {
namespace Detail {

class MtPipelineLay;
class MtDevice;

class MtDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    MtDescriptorArray(MtDevice& dev, const MtPipelineLay &lay);

    void set    (size_t id, AbstractGraphicsApi::Texture* tex, const Sampler2d& smp) override;
    void setSsbo(size_t id, AbstractGraphicsApi::Texture* tex, uint32_t mipLevel) override;
    void setUbo (size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset) override;
    void setSsbo(size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset) override;
    void ssboBarriers(Detail::ResourceState& res) override;

    struct Desc {
      id       val;
      id       sampler;
      union {
        size_t offset = 0;
        size_t mipLevel;
        };
      };

    MtDevice&                        dev;
    std::unique_ptr<Desc[]>          desc;
    DSharedPtr<const MtPipelineLay*> lay;
  };

}
}


