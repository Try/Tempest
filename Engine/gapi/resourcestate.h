#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

namespace Tempest {
namespace Detail {

class ResourceState {
  public:
    ResourceState() = default;

    void setLayout  (AbstractGraphicsApi::Swapchain& s, uint32_t id, ResourceLayout lay, bool preserve);
    void setLayout  (AbstractGraphicsApi::Texture&   a, ResourceLayout lay, bool preserve);
    void setLayout  (AbstractGraphicsApi::Buffer&    b, ResourceLayout lay);

    void flush      (AbstractGraphicsApi::CommandBuffer& cmd);
    void finalize   (AbstractGraphicsApi::CommandBuffer& cmd);

  private:
    struct State {
      AbstractGraphicsApi::Swapchain* sw = nullptr;
      uint32_t                        id = 0;
      AbstractGraphicsApi::Texture*   img = nullptr;

      ResourceLayout                  last;
      ResourceLayout                  next;
      bool                            preserve = false;

      bool                            outdated;
      };

    struct BufState {
      AbstractGraphicsApi::Buffer* buf = nullptr;
      ResourceLayout               last;
      ResourceLayout               next;
      bool                         outdated;
      };

    State&    findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id, ResourceLayout def, bool preserve);
    BufState& findBuf(AbstractGraphicsApi::Buffer*  buf);

    std::vector<State>    imgState;
    std::vector<BufState> bufState;
  };

}
}

