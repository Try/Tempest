#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VRenderPass;
class VFramebuffer;

class VDevice;
class VCommandPool;

class VPipeline;
class VBuffer;
class VTexture;
class VImage;

class VCommandBuffer:public AbstractGraphicsApi::CommandBuffer {
  public:
     enum Usage : uint32_t {
      ONE_TIME_SUBMIT_BIT      = 0x00000001,
      RENDER_PASS_CONTINUE_BIT = 0x00000002,
      SIMULTANEOUS_USE_BIT     = 0x00000004,
      };

    VCommandBuffer()=delete;
    VCommandBuffer(VDevice &device, VCommandPool &pool, CmdType secondary);
    VCommandBuffer(VCommandBuffer&& other);
    ~VCommandBuffer();

    void operator=(VCommandBuffer&& other);

    VkCommandBuffer impl=nullptr;

    void reset();

    void begin(Usage usageFlags);
    void begin(Usage usageFlags, VRenderPass& rpass);

    void begin();
    void begin(AbstractGraphicsApi::Pass* p);
    void next (AbstractGraphicsApi::Pass* p);
    void end();

    void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                         AbstractGraphicsApi::Pass*  p,
                         uint32_t width,uint32_t height);
    void beginSecondaryPass(AbstractGraphicsApi::Fbo* f,
                            AbstractGraphicsApi::Pass*  p,
                            uint32_t width,uint32_t height);
    void endRenderPass();

    void clear(AbstractGraphicsApi::Image& img, float r, float g, float b, float a);
    void setPipeline(AbstractGraphicsApi::Pipeline& p);
    void setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u, size_t offc, const uint32_t* offv);
    void setViewport(const Rect& r);

    void exec(const AbstractGraphicsApi::CommandBuffer& buf);

    void draw(size_t offset, size_t size);
    void drawIndexed(size_t ioffset, size_t isize, size_t voffset);

    void setVbo(const AbstractGraphicsApi::Buffer& b);
    void setIbo(const AbstractGraphicsApi::Buffer* b,Detail::IndexClass cls);

    void flush(const Detail::VBuffer& src, size_t size);
    void copy(Detail::VBuffer&  dest, size_t offsetDest, const Detail::VBuffer& src, size_t offsetSrc, size_t size);
    void copy(Detail::VTexture& dest, size_t width, size_t height, const Detail::VBuffer& src);

    void changeLayout(AbstractGraphicsApi::Texture& t,TextureLayout prev,TextureLayout next);
    void changeLayout(Detail::VTexture& dest, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount);
    void generateMipmap(VTexture& image, VkPhysicalDevice pdev, VkFormat imageFormat,
                        uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels);

  private:
    VkDevice        device=nullptr;
    VkCommandPool   pool  =VK_NULL_HANDLE;
  };

}}
