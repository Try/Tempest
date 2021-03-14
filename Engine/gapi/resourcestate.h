#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

namespace Tempest {
namespace Detail {

class ResourceState {
  public:
    ResourceState() = default;

    void setLayout  (AbstractGraphicsApi::Attach& a, TextureLayout lay, bool preserve);
    void setLayout  (AbstractGraphicsApi::Buffer& b, BufferLayout  lay);

    void flushLayout(AbstractGraphicsApi::CommandBuffer& cmd);
    void flushSSBO  (AbstractGraphicsApi::CommandBuffer& cmd);
    void finalize   (AbstractGraphicsApi::CommandBuffer& cmd);

  private:
    struct State {
      AbstractGraphicsApi::Attach* img = nullptr;
      TextureLayout                last;
      TextureLayout                next;
      bool                         outdated;
      };

    struct BufState {
      AbstractGraphicsApi::Buffer* buf = nullptr;
      BufferLayout                 last;
      BufferLayout                 next;
      bool                         outdated;
      };

    State&    findImg(AbstractGraphicsApi::Attach* img, bool preserve);
    BufState& findBuf(AbstractGraphicsApi::Buffer* buf);

    std::vector<State>    imgState;
    std::vector<BufState> bufState;
  };

}
}

