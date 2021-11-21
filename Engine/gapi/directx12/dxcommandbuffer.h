#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include <vector>

#include "comptr.h"
#include "gapi/resourcestate.h"
#include "dxfbolayout.h"
#include "dxpipelinelay.h"

namespace Tempest {

class DirectX12Api;

namespace Detail {

class DxDevice;
class DxBuffer;
class DxTexture;

class DxCommandBuffer:public AbstractGraphicsApi::CommandBuffer {
  public:
    enum RpState : uint8_t {
      NoRecording,
      Idle,
      RenderPass,
      Compute,
      };
    DxCommandBuffer(DxDevice& d);
    ~DxCommandBuffer();

    using AbstractGraphicsApi::CommandBuffer::barrier;

    void begin() override;
    void end()   override;
    void reset() override;

    bool isRecording() const override;

    void beginRendering(const AttachmentDesc* desc, size_t descSize,
                        uint32_t w, uint32_t h,
                        const TextureFormat* frm,
                        AbstractGraphicsApi::Texture** att,
                        AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) override;
    void endRendering() override;

    void setViewport (const Rect& r) override;
    void setScissor  (const Rect& r) override;

    void setPipeline (AbstractGraphicsApi::Pipeline& p) override;
    void setBytes    (AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) override;
    void setUniforms (AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc& u) override;

    void setComputePipeline(AbstractGraphicsApi::CompPipeline& p) override;
    void setBytes    (AbstractGraphicsApi::CompPipeline& p, const void* data, size_t size) override;
    void setUniforms (AbstractGraphicsApi::CompPipeline& p, AbstractGraphicsApi::Desc& u) override;

    void draw        (const AbstractGraphicsApi::Buffer& vbo,
                      size_t offset,size_t vertexCount, size_t firstInstance, size_t instanceCount) override;
    void drawIndexed (const AbstractGraphicsApi::Buffer& vbo, const AbstractGraphicsApi::Buffer& ibo, Detail::IndexClass cls,
                      size_t ioffset, size_t isize, size_t voffset, size_t firstInstance, size_t instanceCount) override;
    void dispatch    (size_t x, size_t y, size_t z) override;

    void barrier     (const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) override;

    void copy(AbstractGraphicsApi::Buffer& dst, size_t offset, AbstractGraphicsApi::Texture& src, uint32_t width, uint32_t height, uint32_t mip) override;
    void generateMipmap(AbstractGraphicsApi::Texture& image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) override;

    void copyNative(AbstractGraphicsApi::Buffer& dest, size_t offset, const AbstractGraphicsApi::Texture& src, uint32_t width, uint32_t height, uint32_t mip);
    void copy(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, const AbstractGraphicsApi::Buffer& src, size_t offsetSrc, size_t size);
    void copy(AbstractGraphicsApi::Texture& dest, size_t width, size_t height, size_t mip, const AbstractGraphicsApi::Buffer&  src, size_t offset);

    ID3D12GraphicsCommandList* get() { return impl.get(); }

  private:
    DxDevice&                          dev;
    ComPtr<ID3D12CommandAllocator>     pool;
    ComPtr<ID3D12GraphicsCommandList4> impl;
    bool                               resetDone=false;

    DxFboLayout                        fboLayout;

    ID3D12DescriptorHeap*              currentHeaps[DxPipelineLay::MAX_BINDS]={};
    DxDescriptorArray*                 curUniforms = nullptr;

    UINT                               vboStride=0;

    ResourceState                      resState;
    RpState                            state        = NoRecording;
    bool                               ssboBarriers = false;

    struct Stage {
      virtual ~Stage() = default;
      virtual void exec(DxCommandBuffer& cmd) = 0;
      Stage* next = nullptr;
      };
    struct Blit;
    struct MipMaps;
    struct CopyBuf;
    Stage*                            stageResources = nullptr;

    void clearStage();
    void pushStage(Stage* cmd);

    void blitFS(AbstractGraphicsApi::Texture& src, uint32_t srcW, uint32_t srcH, uint32_t srcMip,
                AbstractGraphicsApi::Texture& dst, uint32_t dstW, uint32_t dstH, uint32_t dstMip);

    void implSetUniforms(AbstractGraphicsApi::Desc& u, bool isCompute);
  };

}
}

