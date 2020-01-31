#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

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
      Pending,
      Inline,
      Secondary
      };

    VCommandBuffer()=delete;
    VCommandBuffer(VDevice &device, VCommandPool &pool, VFramebufferLayout* fbo, CmdType secondary);
    VCommandBuffer(VCommandBuffer&& other);
    ~VCommandBuffer();

    VCommandBuffer& operator=(VCommandBuffer&& other);

    VkCommandBuffer impl=nullptr;

    void reset();

    void begin(Usage usage); // internal
    void begin();
    void end();
    bool isRecording() const;

    void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                         AbstractGraphicsApi::Pass*  p,
                         uint32_t width,uint32_t height);
    void endRenderPass();

    void setPipeline(AbstractGraphicsApi::Pipeline& p, uint32_t w, uint32_t h);
    void setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u, size_t offc, const uint32_t* offv);
    void setViewport(const Rect& r);

    void exec(const AbstractGraphicsApi::CommandBuffer& buf);

    void draw(size_t offset, size_t size);
    void drawIndexed(size_t ioffset, size_t isize, size_t voffset);

    void setVbo(const AbstractGraphicsApi::Buffer& b);
    void setIbo(const AbstractGraphicsApi::Buffer* b,Detail::IndexClass cls);

    void flush(const Detail::VBuffer& src, size_t size);
    void copy(Detail::VBuffer&  dest, size_t offsetDest, const Detail::VBuffer& src, size_t offsetSrc, size_t size);
    void copy(Detail::VTexture& dest, size_t width, size_t height, size_t mip, const Detail::VBuffer&  src, size_t offset);
    void copy(Detail::VBuffer&  dest, size_t width, size_t height, size_t mip, const Detail::VTexture& src, size_t offset);

    void changeLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, TextureFormat frm, TextureLayout prev, TextureLayout next);
    void changeLayout(AbstractGraphicsApi::Texture& t, TextureFormat frm, TextureLayout prev, TextureLayout next);
    void changeLayout(VkImage dest, VkFormat imageFormat, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount, bool byRegion);

    void generateMipmap(VTexture& image, VkPhysicalDevice pdev, VkFormat imageFormat,
                        uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels);

  private:
    struct ImgState;
    struct BuildState;
    struct Secondarys;

    VkDevice                                device=nullptr;
    VkCommandPool                           pool  =VK_NULL_HANDLE;
    std::unique_ptr<BuildState>             bstate;
    // prime cmd buf
    std::vector<VkCommandBuffer>            chunks;
    // secondary cmd buf
    Detail::DSharedPtr<VFramebufferLayout*> fboLay;

    enum CmdBuff {
      CmdInline,
      CmdRenderpass,
      };
    bool            isSecondary() const;
    VkCommandBuffer getBuffer(CmdBuff cmd);
    void            implSetUniforms(VkCommandBuffer cmd, VPipeline& p, VDescriptorArray& u, size_t offc, const uint32_t* offv, uint32_t* buf);

  };

}}
