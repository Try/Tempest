#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include <vector>

#include "comptr.h"
#include "gapi/resourcestate.h"
#include "dxframebuffer.h"
#include "dxuniformslay.h"

namespace Tempest {

class DirectX12Api;

namespace Detail {

class DxDevice;
class DxBuffer;
class DxTexture;
class DxFramebuffer;
class DxFboLayout;
class DxRenderPass;

class DxCommandBuffer:public AbstractGraphicsApi::CommandBuffer {
  public:
    DxCommandBuffer(DxDevice& d);
    ~DxCommandBuffer();

    void begin() override;
    void end()   override;
    void reset();

    bool isRecording() const override;
    void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                         AbstractGraphicsApi::Pass*  p,
                         uint32_t width,uint32_t height) override;
    void endRenderPass() override;

    void setViewport (const Rect& r) override;

    void setPipeline (AbstractGraphicsApi::Pipeline& p,uint32_t w,uint32_t h) override;
    void setBytes    (AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) override;
    void setUniforms (AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc& u) override;

    void setComputePipeline(AbstractGraphicsApi::CompPipeline& p) override;
    void setBytes    (AbstractGraphicsApi::CompPipeline& p, const void* data, size_t size) override;
    void setUniforms (AbstractGraphicsApi::CompPipeline& p, AbstractGraphicsApi::Desc& u) override;

    void changeLayout(AbstractGraphicsApi::Attach&  img, TextureLayout prev, TextureLayout next, bool byRegion) override;

    void changeLayout(AbstractGraphicsApi::Texture& t, TextureLayout prev, TextureLayout next, uint32_t mipCnt);
    void setVbo      (const AbstractGraphicsApi::Buffer& b) override;
    void setIbo      (const AbstractGraphicsApi::Buffer& b, Detail::IndexClass cls) override;
    void draw        (size_t offset,size_t vertexCount) override;
    void drawIndexed (size_t ioffset, size_t isize, size_t voffset) override;
    void dispatch    (size_t x, size_t y, size_t z) override;

    void copy(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, const AbstractGraphicsApi::Buffer& src, size_t offsetSrc, size_t size);
    void copy(AbstractGraphicsApi::Texture& dest, size_t width, size_t height, size_t mip, const AbstractGraphicsApi::Buffer&  src, size_t offset);
    void copy(AbstractGraphicsApi::Buffer&  dest, size_t width, size_t height, size_t mip, const AbstractGraphicsApi::Texture& src, size_t offset);
    void generateMipmap(AbstractGraphicsApi::Texture& image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels);

    ID3D12GraphicsCommandList* get() { return impl.get(); }

  private:
    DxDevice&                         dev;
    ComPtr<ID3D12CommandAllocator>    pool;
    ComPtr<ID3D12GraphicsCommandList> impl;
    bool                              recording=false;
    bool                              resetDone=false;

    DxFramebuffer*                    currentFbo  = nullptr;
    DxRenderPass*                     currentPass = nullptr;
    ID3D12DescriptorHeap*             currentHeaps[DxUniformsLay::MAX_BINDS]={};

    UINT                              vboStride=0;

    ResourceState                     resState;

    void implSetUniforms(AbstractGraphicsApi::Desc& u, bool isCompute);
    void implChangeLayout(ID3D12Resource* res, D3D12_RESOURCE_STATES prev, D3D12_RESOURCE_STATES lay);

    friend class Tempest::DirectX12Api;
  };

}
}

