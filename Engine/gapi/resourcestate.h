#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

namespace Tempest {
namespace Detail {

class ResourceState {
  public:
    ResourceState();

    static constexpr const uint32_t AllMips = 0xFFFFFFFF;

    struct Usage {
      NonUniqResId read  = NonUniqResId::I_None;
      NonUniqResId write = NonUniqResId::I_None;

      bool         speculative = false;
      };

    void setRenderpass(AbstractGraphicsApi::CommandBuffer& cmd,
                       const AttachmentDesc* desc, size_t descSize,
                       const TextureFormat* frm,
                       AbstractGraphicsApi::Texture** att,
                       AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId);
    void setLayout  (AbstractGraphicsApi::Swapchain& s, uint32_t id, ResourceLayout lay, bool discard);
    void setLayout  (AbstractGraphicsApi::Texture&   a, ResourceLayout lay, uint32_t mip, bool discard = false);

    void onTranferUsage(NonUniqResId read, NonUniqResId write, bool host);
    void onDrawUsage   (NonUniqResId id, AccessOp loadOp);
    void onUavUsage    (NonUniqResId read, NonUniqResId write, PipelineStage st, bool host = false);
    void onUavUsage    (const ResourceState::Usage& uavUsage, PipelineStage st, bool host = false);
    void forceLayout   (AbstractGraphicsApi::Texture&   a, ResourceLayout lay);

    void joinWriters(PipelineStage st);
    void clearReaders();
    void flush      (AbstractGraphicsApi::CommandBuffer& cmd);
    void finalize   (AbstractGraphicsApi::CommandBuffer& cmd);

  private:
    struct ImgState {
      AbstractGraphicsApi::Swapchain* sw       = nullptr;
      AbstractGraphicsApi::Texture*   img      = nullptr;
      uint32_t                        id       = 0; // mip or image-id in swapchain

      ResourceLayout                  last     = ResourceLayout::None;
      ResourceLayout                  next     = ResourceLayout::None;
      bool                            discard  = false;
      };

    void      fillReads();
    ImgState& findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id, bool discard);
    void      emitBarriers(AbstractGraphicsApi::CommandBuffer& cmd, AbstractGraphicsApi::SyncDesc& d, AbstractGraphicsApi::BarrierDesc* desc, size_t cnt);

    std::vector<ImgState> imgState;

    struct Stage {
      NonUniqResId depend[PipelineStage::S_Count];
      };
    Stage     uavRead [PipelineStage::S_Count] = {};
    Stage     uavWrite[PipelineStage::S_Count] = {};
    SyncStage uavSrcBarrier = SyncStage::None;
    SyncStage uavDstBarrier = SyncStage::None;
  };

}
}

