#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

namespace Tempest {
namespace Detail {

class ResourceState {
  public:
    ResourceState();

    struct Usage {
      NonUniqResId read  = NonUniqResId::I_None;
      NonUniqResId write = NonUniqResId::I_None;
      bool         durty = false;
      };

    void setRenderpass(AbstractGraphicsApi::CommandBuffer& cmd,
                       const AttachmentDesc* desc, size_t descSize,
                       const TextureFormat* frm,
                       AbstractGraphicsApi::Texture** att,
                       AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId);
    void setLayout  (AbstractGraphicsApi::Swapchain& s, uint32_t id, ResourceAccess lay, bool discard);
    void setLayout  (AbstractGraphicsApi::Texture&   a, ResourceAccess lay, bool discard = false);
    void setLayout  (const AbstractGraphicsApi::Buffer& a, ResourceAccess lay);

    void onTranferUsage(NonUniqResId read, NonUniqResId write, bool host);
    void onUavUsage    (NonUniqResId read, NonUniqResId write, PipelineStage st, bool host = false);
    void onUavUsage    (const ResourceState::Usage& uavUsage, PipelineStage st, bool host = false);
    void forceLayout   (AbstractGraphicsApi::Texture&   a);

    void joinWriters(PipelineStage st);
    void clearReaders();
    void flush      (AbstractGraphicsApi::CommandBuffer& cmd);
    void finalize   (AbstractGraphicsApi::CommandBuffer& cmd);

  private:
    struct ImgState {
      AbstractGraphicsApi::Swapchain* sw       = nullptr;
      uint32_t                        id       = 0;
      AbstractGraphicsApi::Texture*   img      = nullptr;

      ResourceAccess                  last     = ResourceAccess::None;
      ResourceAccess                  next     = ResourceAccess::None;
      bool                            discard  = false;

      bool                            outdated = false;
      };

    struct BufState {
      const AbstractGraphicsApi::Buffer* buf      = nullptr;
      ResourceAccess                     last     = ResourceAccess::None;
      ResourceAccess                     next     = ResourceAccess::None;
      bool                               outdated = false;
      };

    void      fillReads();
    ImgState& findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id, ResourceAccess def, bool discard);
    BufState& findBuf(const AbstractGraphicsApi::Buffer*  buf, ResourceAccess def);
    void      emitBarriers(AbstractGraphicsApi::CommandBuffer& cmd, AbstractGraphicsApi::BarrierDesc* desc, size_t cnt);

    std::vector<ImgState> imgState;
    std::vector<BufState> bufState;

    struct Stage {
      NonUniqResId depend[PipelineStage::S_Count];
      };
    Stage                 uavRead [PipelineStage::S_Count] = {};
    Stage                 uavWrite[PipelineStage::S_Count] = {};
    ResourceAccess        uavSrcBarrier = ResourceAccess::None;
    ResourceAccess        uavDstBarrier = ResourceAccess::None;
  };

}
}

