#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "gapi/vulkan/vcommandpool.h"
#include "gapi/vulkan/vframebuffermap.h"
#include "gapi/vulkan/vbindlesscache.h"
#include "gapi/vulkan/vpushdescriptor.h"
#include "gapi/vulkan/vswapchain.h"
#include "gapi/resourcestate.h"

#include "../utility/smallarray.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VCommandPool;

class VPipeline;
class VCompPipeline;
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

    void begin(SyncHint hint) override;
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
    void setComputePipeline(AbstractGraphicsApi::CompPipeline& p) override;

    void setPushData(const void* data, size_t size) override;
    void setBinding (size_t id, AbstractGraphicsApi::Texture*   tex, uint32_t mipLevel, const ComponentMapping& m, const Sampler& smp) override;
    void setBinding (size_t id, AbstractGraphicsApi::Buffer*    buf, size_t offset) override;
    void setBinding (size_t id, AbstractGraphicsApi::DescArray* arr) override;
    void setBinding (size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) override;
    void setBinding (size_t id, const Sampler& smp) override;

    void draw       (const AbstractGraphicsApi::Buffer* vbo, size_t stride, size_t voffset, size_t vsize, size_t firstInstance, size_t instanceCount) override;
    void drawIndexed(const AbstractGraphicsApi::Buffer* vbo, size_t stride, size_t voffset,
                     const AbstractGraphicsApi::Buffer& ibo, Detail::IndexClass cls,
                     size_t ioffset, size_t isize, size_t firstInstance, size_t instanceCount) override;

    void drawIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) override;

    void dispatchMesh(size_t x, size_t y, size_t z) override;
    void dispatchMeshIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) override;

    void dispatch    (size_t x, size_t y, size_t z) override;
    void dispatchIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) override;

    void discard(AbstractGraphicsApi::Texture& tex);
    void barrier(const AbstractGraphicsApi::SyncDesc& s, const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) override;

    void generateMipmap(AbstractGraphicsApi::Texture& image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) override;

    void copy(AbstractGraphicsApi::Texture& dest, size_t width, size_t height, size_t mip, const AbstractGraphicsApi::Buffer&  src, size_t offset);
    void copy(AbstractGraphicsApi::Buffer&  dest, size_t offset, AbstractGraphicsApi::Texture& src, uint32_t width, uint32_t height, uint32_t mip) override;
    void copy(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, const AbstractGraphicsApi::Buffer& src, size_t offsetSrc, size_t size);
    void copy(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, const void* src, size_t size);

    void fill(AbstractGraphicsApi::Texture& dest, uint32_t val);
    void fill(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, uint32_t val, size_t size);

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
    void vkCmdBeginRenderingKHR(VkCommandBuffer impl, const VkRenderingInfo* info);
    void vkCmdEndRenderingKHR(VkCommandBuffer impl);

    void blit(AbstractGraphicsApi::Texture& src, uint32_t srcW, uint32_t srcH, uint32_t srcMip,
              AbstractGraphicsApi::Texture& dst, uint32_t dstW, uint32_t dstH, uint32_t dstMip);

    template<class T>
    void finalizeImageBarrier(T& bx, const AbstractGraphicsApi::BarrierDesc& desc);

    void pushChunk();
    void newChunk();

    void bindVbo(const VBuffer& vbo, size_t stride);
    void implSetUniforms(const PipelineStage st);
    void implSetPushData(const PipelineStage st);
    void handleSync(const ShaderReflection::LayoutDesc& lay, const ShaderReflection::SyncDesc& sync, PipelineStage st);

    struct PipelineInfo:VkPipelineRenderingCreateInfoKHR {
      VkFormat colorFrm[MaxFramebufferAttachments];
      };

    struct Push {
      uint8_t            data[256] = {};
      uint8_t            size      = 0;
      bool               durty     = false;
      };

    struct Bindings : Detail::Bindings {
      NonUniqResId read  = NonUniqResId::I_None;
      NonUniqResId write = NonUniqResId::I_None;
      bool         host  = false;
      bool         durty = false;
      };

    VDevice&                                device;
    VCommandPool                            pool;
    VkCommandBuffer                         impl=nullptr;

    ResourceState                           resState;
    std::shared_ptr<VFramebufferMap::RenderPass> passRp;
    PipelineInfo                            passDyn = {};

    Push                                    pushData;
    Bindings                                bindings;
    VPushDescriptor                         pushDescriptors;

    RpState                                 state           = NoRecording;
    VPipeline*                              curDrawPipeline = nullptr;
    VCompPipeline*                          curCompPipeline = nullptr;
    VkBuffer                                curVbo          = VK_NULL_HANDLE;
    size_t                                  vboStride       = 0;
    VkPipelineLayout                        pipelineLayout  = VK_NULL_HANDLE;

    bool                                    isDbgRegion = false;
  };

}}
