#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

namespace Tempest {
namespace Detail {

class ResourceState {
  public:
    ResourceState() = default;

    void setLayout  (AbstractGraphicsApi::Swapchain& s, uint32_t id, TextureLayout lay, bool preserve);
    void setLayout  (AbstractGraphicsApi::Texture&   a, TextureLayout lay, bool preserve);
    void setLayout  (AbstractGraphicsApi::Buffer&    b, BufferLayout  lay);

    void flushLayout(AbstractGraphicsApi::CommandBuffer& cmd);
    void flushSSBO  (AbstractGraphicsApi::CommandBuffer& cmd);
    void finalize   (AbstractGraphicsApi::CommandBuffer& cmd);

  private:
    struct State {
      AbstractGraphicsApi::Swapchain* sw = nullptr;
      uint32_t                        id = 0;
      AbstractGraphicsApi::Texture*   img = nullptr;

      TextureLayout                   last;
      TextureLayout                   next;
      bool                            preserve = false;

      bool                            outdated;
      };

    struct BufState {
      AbstractGraphicsApi::Buffer* buf = nullptr;
      BufferLayout                 last;
      BufferLayout                 next;
      bool                         outdated;
      };

    State&    findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id, TextureLayout def, bool preserve);
    BufState& findBuf(AbstractGraphicsApi::Buffer*  buf);

    std::vector<State>    imgState;
    std::vector<BufState> bufState;
  };

}
}

