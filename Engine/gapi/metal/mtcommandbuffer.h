#pragma once

#include <Tempest/AbstractGraphicsApi>
#import  <Metal/MTLStageInputOutputDescriptor.h>
#import  <Metal/MTLCommandBuffer.h>
#import  <Metal/MTLRenderCommandEncoder.h>

#include "gapi/shaderreflection.h"

#include "mtpipelinelay.h"

namespace Tempest {

class MetalApi;

namespace Detail {

class MtDevice;
class MtBuffer;
class MtFramebuffer;

class MtCommandBuffer : public AbstractGraphicsApi::CommandBuffer {
  public:
    MtCommandBuffer(MtDevice &dev);
    ~MtCommandBuffer();

    void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                         AbstractGraphicsApi::Pass*  p,
                         uint32_t width,uint32_t height) override;
    void endRenderPass() override;
    void beginCompute() override;
    void endCompute() override;

    void changeLayout  (AbstractGraphicsApi::Buffer& buf, BufferLayout prev, BufferLayout next) override;
    void changeLayout  (AbstractGraphicsApi::Attach& img, TextureLayout prev, TextureLayout next, bool byRegion) override;
    void generateMipmap(AbstractGraphicsApi::Texture& image, TextureLayout defLayout, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) override;

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

    void setVbo      (const AbstractGraphicsApi::Buffer &b) override;
    void setIbo      (const AbstractGraphicsApi::Buffer& b,Detail::IndexClass cls) override;
    void draw        (size_t offset,size_t vertexCount, size_t firstInstance, size_t instanceCount) override;
    void drawIndexed (size_t ioffset, size_t isize, size_t voffset, size_t firstInstance, size_t instanceCount) override;
    void dispatch    (size_t x, size_t y, size_t z) override;

  private:
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

    uint32_t             curVboId = 0;
    MtFramebuffer*       curFbo   = nullptr;
    const MtPipelineLay* curLay   = nullptr;
    const MtBuffer*      curIbo   = nullptr;
    MTLIndexType         iboType  = MTLIndexTypeUInt16;
    MTLPrimitiveType     topology = MTLPrimitiveTypePoint;

    uint32               maxTotalThreadsPerThreadgroup = 0;

  friend class Tempest::MetalApi;
  };

}
}
