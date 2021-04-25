#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {
namespace Detail {

class MtRenderPass : public AbstractGraphicsApi::Pass {
  public:
    MtRenderPass(const FboMode **att, size_t acount);

    bool isCompatible(const MtRenderPass& other) const;
    std::vector<FboMode> mode;
  };

}
}
