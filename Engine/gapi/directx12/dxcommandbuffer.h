#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include <vector>

#include "comptr.h"

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
    void reset();

    bool isRecording() const override;
    void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                         AbstractGraphicsApi::Pass*  p,
                         uint32_t width,uint32_t height) override;
    void endRenderPass() override;

    void setPipeline (AbstractGraphicsApi::Pipeline& p,uint32_t w,uint32_t h) override;
    void setViewport (const Rect& r) override;
    void setUniforms (AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc& u, size_t offc, const uint32_t* offv) override;
    void exec        (const AbstractGraphicsApi::CommandBundle& buf) override;
    void changeLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, TextureFormat frm, TextureLayout prev, TextureLayout next) override;
    void changeLayout(AbstractGraphicsApi::Texture& t,TextureFormat frm,TextureLayout prev,TextureLayout next) override;
    void setVbo      (const AbstractGraphicsApi::Buffer& b) override;
    void setIbo      (const AbstractGraphicsApi::Buffer* b,Detail::IndexClass cls) override;
    void draw        (size_t offset,size_t vertexCount) override;
    void drawIndexed (size_t ioffset, size_t isize, size_t voffset) override;

    void flush(const Detail::DxBuffer& src, size_t size);
    void copy(Detail::DxBuffer&  dest, size_t offsetDest, const Detail::DxBuffer& src, size_t offsetSrc, size_t size);
    void copy(Detail::DxTexture& dest, size_t width, size_t height, size_t mip, const Detail::DxBuffer&  src, size_t offset);
    void copy(Detail::DxBuffer&  dest, size_t width, size_t height, size_t mip, const Detail::DxTexture& src, size_t offset);

    ID3D12GraphicsCommandList* get() { return impl.get(); }

  private:
    struct ImgState;

    DxDevice&                         dev;
    ComPtr<ID3D12CommandAllocator>    pool;
    ComPtr<ID3D12GraphicsCommandList> impl;
    bool                              recording=false;

    UINT                              vboStride=0;

    std::vector<ImgState>             imgState;

    void                       setLayout(ID3D12Resource* res, D3D12_RESOURCE_STATES lay);
    void                       implChangeLayout(ID3D12Resource* res, D3D12_RESOURCE_STATES prev, D3D12_RESOURCE_STATES lay);
    DxCommandBuffer::ImgState& findImg(ID3D12Resource* img, D3D12_RESOURCE_STATES last);
    void                       flushLayout();

    friend class Tempest::DirectX12Api;
  };

}
}

