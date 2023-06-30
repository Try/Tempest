#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/Metal.hpp>

#include "gapi/shaderreflection.h"
#include "mtfbolayout.h"
#include "mtpipelinelay.h"
#include "nsptr.h"

namespace Tempest {

class MetalApi;

namespace Detail {

class MtDevice;
class MtBuffer;
class MtPipeline;
class MtDescriptorArray;

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
    void setScissor (const Rect& r) override;
    void setDebugMarker(std::string_view tag) override;

    void draw        (size_t vsize, size_t firstInstance, size_t instanceCount) override;
    void draw        (const AbstractGraphicsApi::Buffer& vbo, size_t stride, size_t voffset,size_t vertexCount,
                      size_t firstInstance, size_t instanceCount) override;
    void drawIndexed (const AbstractGraphicsApi::Buffer& vbo, size_t stride, size_t voffset,
                      const AbstractGraphicsApi::Buffer &ibo, Detail::IndexClass cls,
                      size_t ioffset, size_t isize, size_t firstInstance, size_t instanceCount) override;
    void dispatchMesh(size_t x, size_t y, size_t z) override;
    void dispatch    (size_t x, size_t y, size_t z) override;

    void barrier       (const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) override;
    void generateMipmap(AbstractGraphicsApi::Texture& image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) override;
    void copy          (AbstractGraphicsApi::Buffer& dst, size_t offset,
                        AbstractGraphicsApi::Texture& src, uint32_t width, uint32_t height, uint32_t mip) override;

  private:
    enum EncType:uint8_t {
      E_None = 0,
      E_Draw,
      E_Comp,
      E_Blit,
      };
    void setEncoder(EncType e, MTL::RenderPassDescriptor* desc);
    void implSetBytes   (const void* bytes, size_t sz);
    void implSetUniforms(AbstractGraphicsApi::Desc& u);
    void implSetAlux    (MtDescriptorArray& u);

    void setBuffer (const MtPipelineLay::MTLBind& mtl, MTL::Buffer* b, size_t offset);
    void setTexture(const MtPipelineLay::MTLBind& mtl, MTL::Texture* t, MTL::SamplerState* ss);
    void setTlas   (const MtPipelineLay::MTLBind& mtl, MTL::AccelerationStructure* as);

    MtDevice&                         device;
    NsPtr<MTL::CommandBuffer>         impl;

    NsPtr<MTL::RenderCommandEncoder>  encDraw;
    NsPtr<MTL::ComputeCommandEncoder> encComp;
    NsPtr<MTL::BlitCommandEncoder>    encBlit;

    MtFboLayout                       curFbo;
    MtPipeline*                       curDrawPipeline = nullptr;
    size_t                            vboStride       = 0;
    const MtPipelineLay*              curLay          = nullptr;
    uint32_t                          curVboId        = 0;
    MTL::Size                         localSize       = {};
    MTL::Size                         localSizeMesh   = {};
    MTL::PrimitiveType                topology        = MTL::PrimitiveTypePoint;
    bool                              isTesselation   = false;

    uint32_t                          maxTotalThreadsPerThreadgroup = 0;
    bool                              isDbgRegion = false;

  friend class Tempest::MetalApi;
  };

}
}
