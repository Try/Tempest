#pragma once

#include <Tempest/AbstractGraphicsApi>
#import  <Metal/MTLStageInputOutputDescriptor.h>
#import  <Metal/MTLCommandBuffer.h>
#import  <Metal/MTLRenderCommandEncoder.h>

#include "gapi/shaderreflection.h"

#include "mtfbolayout.h"
#include "mtpipelinelay.h"

namespace Tempest {

class MetalApi;

namespace Detail {

class MtDevice;
class MtBuffer;

class MtCommandBuffer : public AbstractGraphicsApi::CommandBuffer {
  public:
    MtCommandBuffer(MtDevice &dev);
    ~MtCommandBuffer();

    void beginRendering(const AttachmentDesc* desc, size_t descSize,
                        uint32_t w, uint32_t h,
                        const TextureFormat* frm,
                        AbstractGraphicsApi::Texture** att,
                        AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) override;
    void endRendering() override;

    void barrier       (const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) override;
    void generateMipmap(AbstractGraphicsApi::Texture& image, ResourceLayout defLayout, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) override;

    bool isRecording() const override;
    void begin() override;
    void end() override;
    void reset() override;

    void setPipeline(AbstractGraphicsApi::Pipeline& p) override;
    void setComputePipeline(AbstractGraphicsApi::CompPipeline& p) override;

    void setBytes   (AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) override;
    void setUniforms(AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc& u) override;

    void setBytes   (AbstractGraphicsApi::CompPipeline& p, const void* data, size_t size) override;
    void setUniforms(AbstractGraphicsApi::CompPipeline& p, AbstractGraphicsApi::Desc& u) override;

    void setViewport(const Rect& r) override;

    void draw        (const AbstractGraphicsApi::Buffer& vbo, size_t offset,size_t vertexCount, size_t firstInstance, size_t instanceCount) override;
    void drawIndexed (const AbstractGraphicsApi::Buffer& vbo, const AbstractGraphicsApi::Buffer &ibo, Detail::IndexClass cls,
                      size_t ioffset, size_t isize, size_t voffset, size_t firstInstance, size_t instanceCount) override;
    void dispatch    (size_t x, size_t y, size_t z) override;

    void copy        (AbstractGraphicsApi::Buffer& dest, ResourceLayout defLayout, uint32_t width, uint32_t height, uint32_t mip, AbstractGraphicsApi::Texture& src, size_t offset) override;

  private:
    enum EncType:uint8_t {
      E_None = 0,
      E_Draw,
      E_Comp,
      E_Blit,
      };
    void setEncoder(EncType e, MTLRenderPassDescriptor* desc);
    void implSetBytes   (const void* bytes, size_t sz);
    void implSetUniforms(AbstractGraphicsApi::Desc& u);

    void setBuffer (const MtPipelineLay::MTLBind& mtl,
                    id<MTLBuffer>  b, size_t offset);
    void setTexture(const MtPipelineLay::MTLBind& mtl,
                    id<MTLTexture> b, id<MTLSamplerState> ss);

    MtDevice&            device;
    id<MTLCommandBuffer> impl = nil;

    id<MTLRenderCommandEncoder>  encDraw = nil;
    id<MTLComputeCommandEncoder> encComp = nil;
    id<MTLBlitCommandEncoder>    encBlit = nil;

    uint32_t             curVboId = 0;
    MtFboLayout          curFbo;
    const MtPipelineLay* curLay   = nullptr;
    MTLPrimitiveType     topology = MTLPrimitiveTypePoint;

    uint32               maxTotalThreadsPerThreadgroup = 0;

  friend class Tempest::MetalApi;
  };

}
}
