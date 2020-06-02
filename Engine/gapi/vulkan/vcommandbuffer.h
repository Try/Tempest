#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "vcommandbundle.h"
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

    void reset();

    void begin();
    void begin(VkCommandBufferUsageFlags flg);
    void end();
    bool isRecording() const;

    void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                         AbstractGraphicsApi::Pass*  p,
                         uint32_t width,uint32_t height);
    void endRenderPass();

    void exec(const AbstractGraphicsApi::CommandBundle& buf);

    void setPipeline(AbstractGraphicsApi::Pipeline& p, uint32_t w, uint32_t h);
    void setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u, size_t offc, const uint32_t* offv);
    void setViewport(const Rect& r);

    void setVbo(const AbstractGraphicsApi::Buffer& b);
    void setIbo(const AbstractGraphicsApi::Buffer& b, Detail::IndexClass cls);

    void draw(size_t offset, size_t size);
    void drawIndexed(size_t ioffset, size_t isize, size_t voffset);

    void flush(const Detail::VBuffer& src, size_t size);
    void copy(Detail::VBuffer&  dest, size_t offsetDest, const Detail::VBuffer& src, size_t offsetSrc, size_t size);
    void copy(Detail::VTexture& dest, size_t width, size_t height, size_t mip, const Detail::VBuffer&  src, size_t offset);
    void copy(Detail::VBuffer&  dest, size_t width, size_t height, size_t mip, const Detail::VTexture& src, size_t offset);

    void changeLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, TextureFormat frm, TextureLayout prev, TextureLayout next);
    void changeLayout(AbstractGraphicsApi::Texture& t, TextureFormat frm, TextureLayout prev, TextureLayout next);
    void changeLayout(AbstractGraphicsApi::Texture& t, TextureFormat frm, TextureLayout prev, TextureLayout next, uint32_t mipCnt);
    void changeLayout(VkImage dest, VkFormat imageFormat,
                      VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount, bool byRegion);

    void generateMipmap(VTexture& image, TextureFormat imageFormat,
                        uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels);

  private:
    struct ImgState;

    void            flushLastCmd();
    void            flushLayout();

    VCommandBuffer::ImgState&
                    findImg(VkImage img, VkFormat frm, VkImageLayout last, bool preserve);
    void            setLayout(VFramebuffer::Attach& a, VkFormat frm, VkImageLayout lay, bool preserve);

    VCommandBundle& getChunk();

    VDevice&                                device;
    VCommandPool                            pool;

    std::vector<VCommandBundle>             chunks, reserved;
    VCommandBundle*                         lastChunk=nullptr;

    std::vector<ImgState>                   imgState;

    RpState                                 state=NoRecording;
    Detail::DSharedPtr<VFramebufferLayout*> curFbo;
    VkViewport                              viewPort={};
  };

}}
