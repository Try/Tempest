#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "gapi/resourcestate.h"
#include "vcommandpool.h"
#include "vframebuffer.h"
#include "../utility/dptr.h"

namespace Tempest {
namespace Detail {

class VRenderPass;
class VFramebuffer;
class VFramebufferLayout;

class VDevice;
class VCommandPool;

class VDescriptorArray;
class VPipeline;
class VBuffer;
class VTexture;

class VCommandBuffer:public AbstractGraphicsApi::CommandBuffer {
  public:
    enum Usage : uint32_t {
      ONE_TIME_SUBMIT_BIT      = 0x00000001,
      RENDER_PASS_CONTINUE_BIT = 0x00000002,
      SIMULTANEOUS_USE_BIT     = 0x00000004,
      };

    enum RpState : uint8_t {
      NoRecording,
      NoPass,
      RenderPass
      };

    VCommandBuffer()=delete;
    VCommandBuffer(VDevice &device, VkCommandPoolCreateFlags flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    ~VCommandBuffer();

    VkCommandBuffer impl=nullptr;

    void reset() override;

    void begin() override;
    void begin(VkCommandBufferUsageFlags flg);
    void end() override;
    bool isRecording() const override;

    void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                         AbstractGraphicsApi::Pass*  p,
                         uint32_t width,uint32_t height) override;
    void endRenderPass() override;

    void setPipeline(AbstractGraphicsApi::Pipeline& p, uint32_t w, uint32_t h) override;
    void setBytes   (AbstractGraphicsApi::Pipeline &p, const void* data, size_t size) override;
    void setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u) override;

    void setComputePipeline(AbstractGraphicsApi::CompPipeline& p) override;
    void setBytes   (AbstractGraphicsApi::CompPipeline& p, const void* data, size_t size) override;
    void setUniforms(AbstractGraphicsApi::CompPipeline& p, AbstractGraphicsApi::Desc &u) override;

    void setViewport(const Rect& r) override;

    void setVbo(const AbstractGraphicsApi::Buffer& b) override;
    void setIbo(const AbstractGraphicsApi::Buffer& b, Detail::IndexClass cls) override;

    void draw(size_t offset, size_t size) override;
    void drawIndexed(size_t ioffset, size_t isize, size_t voffset) override;
    void dispatch(size_t x, size_t y, size_t z) override;

    void changeLayout(AbstractGraphicsApi::Buffer&  buf, BufferLayout  prev, BufferLayout  next) override;
    void changeLayout(AbstractGraphicsApi::Attach&  img, TextureLayout prev, TextureLayout next, bool byRegion) override;
    void changeLayout(AbstractGraphicsApi::Texture& tex, TextureLayout prev, TextureLayout next, uint32_t mipId);

    void copy(AbstractGraphicsApi::Buffer&  dest, size_t offsetDest, const AbstractGraphicsApi::Buffer& src, size_t offsetSrc, size_t size);
    void copy(AbstractGraphicsApi::Texture& dest, size_t width, size_t height, size_t mip, const AbstractGraphicsApi::Buffer&  src, size_t offset);
    void copy(AbstractGraphicsApi::Buffer&  dest, size_t width, size_t height, size_t mip, const AbstractGraphicsApi::Texture& src, size_t offset);

    void blit(AbstractGraphicsApi::Texture& src, uint32_t srcW, uint32_t srcH, uint32_t srcMip,
              AbstractGraphicsApi::Texture& dst, uint32_t dstW, uint32_t dstH, uint32_t dstMip);
    void generateMipmap(AbstractGraphicsApi::Texture& image, TextureLayout defLayout, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) override;

  private:
    void implChangeLayout(VkImage dest, VkFormat imageFormat,
                          VkImageLayout oldLayout, VkImageLayout newLayout, bool discardOld,
                          uint32_t mipBase, uint32_t mipCount, bool byRegion);

    VDevice&                                device;
    VCommandPool                            pool;

    ResourceState                           resState;

    RpState                                 state       = NoRecording;
    VFramebuffer*                           curFbo      = nullptr;
    VRenderPass*                            curRp       = nullptr;
    VDescriptorArray*                       curUniforms = nullptr;
    VkViewport                              viewPort    = {};
  };

}}
