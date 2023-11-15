#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "gapi/resourcestate.h"
#include "vcommandpool.h"
#include "vframebuffermap.h"
#include "vswapchain.h"

#include "../utility/smallarray.h"
#include "../utility/dptr.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VCommandPool;

class VDescriptorArray;
class VPipeline;
class VBuffer;
class VTexture;

class VCommandBuffer:public AbstractGraphicsApi::CommandBuffer {
  public:
    enum RpState : uint8_t {
      NoRecording,
      Idle,
      RenderPass,
      PostRenderPass,
      Compute,
      };

    VCommandBuffer()=delete;
    VCommandBuffer(VDevice &device, VkCommandPoolCreateFlags flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    ~VCommandBuffer();

    using AbstractGraphicsApi::CommandBuffer::barrier;

    void reset() override;

    void begin(bool tranfer) override;
    void begin() override;
    void end() override;
    bool isRecording() const override;

    void beginRendering(const AttachmentDesc* desc, size_t descSize,
                        uint32_t w, uint32_t h,
                        const TextureFormat* frm,
                        AbstractGraphicsApi::Texture** att,
                        AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) override;
    void endRendering() override;

    void setViewport(const Rect& r) override;
    void setScissor (const Rect& r) override;
    void setDebugMarker(std::string_view tag) override;

    void setPipeline(AbstractGraphicsApi::Pipeline& p) override;
    void setBytes   (AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) override;
    void setUniforms(AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc &u) override;

    void setComputePipeline(AbstractGraphicsApi::CompPipeline& p) override;
    void setBytes   (AbstractGraphicsApi::CompPipeline& p, const void* data, size_t size) override;
    void setUniforms(AbstractGraphicsApi::CompPipeline& p, AbstractGraphicsApi::Desc &u) override;

    void draw       (size_t vsize, size_t firstInstance, size_t instanceCount) override;
    void draw       (const AbstractGraphicsApi::Buffer& vbo, size_t stride, size_t voffset, size_t vsize, size_t firstInstance, size_t instanceCount) override;
    void drawIndexed(const AbstractGraphicsApi::Buffer& vbo, size_t stride, size_t voffset,
                     const AbstractGraphicsApi::Buffer& ibo, Detail::IndexClass cls,
                     size_t ioffset, size_t isize, size_t firstInstance, size_t instanceCount) override;
    void dispatchMesh(size_t x, size_t y, size_t z) override;
    void dispatch    (size_t x, size_t y, size_t z) override;

    void barrier(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) override;

    void copy(AbstractGraphicsApi::Buffer& dst, size_t offset, AbstractGraphicsApi::Texture& src, uint32_t width, uint32_t height, uint32_t mip) override;
    void generateMipmap(AbstractGraphicsApi::Texture& image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) override;

    void copy(AbstractGraphicsApi::Texture& dest, size_t width, size_t height, size_t mip, const AbstractGraphicsApi::Buffer&  src, size_t offset);
    void copy(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, const AbstractGraphicsApi::Buffer& src, size_t offsetSrc, size_t size);
    void copy(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, const void* src, size_t size);
    void fill(AbstractGraphicsApi::Texture& dest, uint32_t val);
    void fill(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, uint32_t val, size_t size);

    void copyNative(AbstractGraphicsApi::Buffer&        dst, size_t offset,
                    const AbstractGraphicsApi::Texture& src, size_t width, size_t height, size_t mip);

    void blit(AbstractGraphicsApi::Texture& src, uint32_t srcW, uint32_t srcH, uint32_t srcMip,
              AbstractGraphicsApi::Texture& dst, uint32_t dstW, uint32_t dstH, uint32_t dstMip);

    void buildBlas(VkAccelerationStructureKHR dest, AbstractGraphicsApi::Buffer& bbo,
                   AbstractGraphicsApi::BlasBuildCtx& ctx, AbstractGraphicsApi::Buffer& scratch);

    void buildTlas(VkAccelerationStructureKHR dest, AbstractGraphicsApi::Buffer& tbo,
                   const AbstractGraphicsApi::Buffer& instances, uint32_t numInstances,
                   AbstractGraphicsApi::Buffer& scratch);

    struct Chunk {
      VkCommandBuffer impl = nullptr;
      };
    Detail::SmallList<Chunk,32>    chunks;
    std::vector<VSwapchain::Sync*> swapchainSync;

  protected:
    void addDependency(VSwapchain& s, size_t imgId);
    void vkCmdPipelineBarrier2(VkCommandBuffer impl, const VkDependencyInfoKHR* info);

    template<class T>
    void finalizeImageBarrier(T& bx, const AbstractGraphicsApi::BarrierDesc& desc);

    void pushChunk();
    void newChunk();

    void bindVbo(const VBuffer& vbo, size_t stride);

    struct PipelineInfo:VkPipelineRenderingCreateInfoKHR {
      VkFormat colorFrm[MaxFramebufferAttachments];
      };

    VDevice&                                device;
    VCommandPool                            pool;
    VkCommandBuffer                         impl=nullptr;
    VkCommandBuffer                         cbHelper=nullptr;

    ResourceState                           resState;
    std::shared_ptr<VFramebufferMap::RenderPass> pass;
    PipelineInfo                            passDyn = {};

    RpState                                 state           = NoRecording;
    VPipeline*                              curDrawPipeline = nullptr;
    AbstractGraphicsApi::Desc*              curUniforms     = nullptr;
    VkBuffer                                curVbo          = VK_NULL_HANDLE;
    size_t                                  vboStride       = 0;
    VkPipelineLayout                        pipelineLayout  = VK_NULL_HANDLE;

    size_t                                  meshIndirectId  = 0;
    bool                                    isDbgRegion = false;
  };

class VMeshCommandBuffer:public VCommandBuffer {
  public:
    using VCommandBuffer::VCommandBuffer;

    void setPipeline(AbstractGraphicsApi::Pipeline& p) override;
    void setBytes   (AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) override;
    void setUniforms(AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc &u) override;

    void dispatchMesh(size_t x, size_t y, size_t z) override;
  friend class VMeshletHelper;
  };

}}
