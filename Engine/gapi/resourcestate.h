#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

namespace Tempest {
namespace Detail {

class ResourceState {
  public:
    ResourceState() = default;

    void setLayout  (AbstractGraphicsApi::Attach& a, TextureLayout lay, bool preserve);
    void flushLayout(AbstractGraphicsApi::CommandBuffer& cmd);
    void finalize   (AbstractGraphicsApi::CommandBuffer& cmd);

  private:
    struct State {
      AbstractGraphicsApi::Attach* img = nullptr;
      TextureLayout                last;

      TextureLayout                next;
      bool                         outdated;
      };

    State& findImg(AbstractGraphicsApi::Attach* img, bool preserve);

    std::vector<State> imgState;
  };

}
}

