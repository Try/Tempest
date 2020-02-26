#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include <vector>

#include "comptr.h"

namespace Tempest {

class DirectX12Api;

namespace Detail {

class DxDevice;

class DxCommandBuffer:public AbstractGraphicsApi::CommandBuffer {
  public:
    DxCommandBuffer(DxDevice& d);
    void begin() override;
    void end()   override;
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

  private:
    struct ImgState;

    DxDevice&                         dev;
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

