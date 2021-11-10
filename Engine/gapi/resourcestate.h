#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

namespace Tempest {
namespace Detail {

class ResourceState {
  public:
    ResourceState() = default;

    void setRenderpass(AbstractGraphicsApi::CommandBuffer& cmd,
                       const AttachmentDesc* desc, size_t descSize,
                       const TextureFormat* frm,
                       AbstractGraphicsApi::Texture** att,
                       AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId);
    void setLayout  (AbstractGraphicsApi::Swapchain& s, uint32_t id, ResourceAccess lay, bool discard);
    void setLayout  (AbstractGraphicsApi::Texture&   a, ResourceAccess lay, bool discard = false);
    void setLayout  (AbstractGraphicsApi::Buffer&    b, ResourceAccess lay);

    void joinCompute();
    void flush      (AbstractGraphicsApi::CommandBuffer& cmd);
    void finalize   (AbstractGraphicsApi::CommandBuffer& cmd);

  private:
    struct State {
      AbstractGraphicsApi::Swapchain* sw       = nullptr;
      uint32_t                        id       = 0;
      AbstractGraphicsApi::Texture*   img      = nullptr;

      ResourceAccess                  last     = ResourceAccess::None;
      ResourceAccess                  next     = ResourceAccess::None;
      bool                            discard  = false;

      bool                            outdated = false;
      };

    struct BufState {
      AbstractGraphicsApi::Buffer* buf      = nullptr;
      ResourceAccess               last     = ResourceAccess::None;
      ResourceAccess               next     = ResourceAccess::None;
      bool                         outdated = false;
      };

    State&    findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id, ResourceAccess def, bool discard);
    void      emitBarriers(AbstractGraphicsApi::CommandBuffer& cmd, AbstractGraphicsApi::BarrierDesc* desc, size_t cnt);

    std::vector<State>    imgState;
    std::vector<BufState> bufState;
  };

}
}

