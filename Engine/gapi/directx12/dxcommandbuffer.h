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
    DxCommandBuffer(DxDevice& d);
    ~DxCommandBuffer();

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

    void changeLayout(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) override;
    void changeLayout(AbstractGraphicsApi::Buffer&  buf, BufferLayout  prev, BufferLayout  next) override;
    void changeLayout(AbstractGraphicsApi::Texture& tex, TextureLayout prev, TextureLayout next, uint32_t mipId);

    void copy(AbstractGraphicsApi::Buffer& dest, TextureLayout defLayout, uint32_t width, uint32_t height, uint32_t mip, AbstractGraphicsApi::Texture& src, size_t offset) override;
    void generateMipmap(AbstractGraphicsApi::Texture& image, TextureLayout defLayout, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) override;

    void copyRaw(AbstractGraphicsApi::Buffer& dest, uint32_t width, uint32_t height, uint32_t mip, const AbstractGraphicsApi::Texture& src, size_t offset);
    void copy(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, const AbstractGraphicsApi::Buffer& src, size_t offsetSrc, size_t size);
    void copy(AbstractGraphicsApi::Texture& dest, size_t width, size_t height, size_t mip, const AbstractGraphicsApi::Buffer&  src, size_t offset);

    ID3D12GraphicsCommandList* get() { return impl.get(); }

  private:
    DxDevice&                         dev;
    ComPtr<ID3D12CommandAllocator>    pool;
    ComPtr<ID3D12GraphicsCommandList> impl;
    bool                              recording=false;
    bool                              resetDone=false;

    DxFboLayout                       fboLayout;

    ID3D12DescriptorHeap*             currentHeaps[DxPipelineLay::MAX_BINDS]={};
    DxDescriptorArray*                curUniforms = nullptr;

    UINT                              vboStride=0;

    ResourceState                     resState;
    bool                              ssboBarriers = false;
    bool                              isInCompute  = false;

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
    void implChangeLayout(ID3D12Resource* res, D3D12_RESOURCE_STATES prev, D3D12_RESOURCE_STATES lay);

    friend class Tempest::DirectX12Api;
  };

}
}

