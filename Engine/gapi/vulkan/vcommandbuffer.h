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
     enum Usage {
      ONE_TIME_SUBMIT_BIT      = 0x00000001,
      RENDER_PASS_CONTINUE_BIT = 0x00000002,
      SIMULTANEOUS_USE_BIT     = 0x00000004,
      };

    VCommandBuffer()=delete;
    VCommandBuffer(VDevice &device,VCommandPool &pool);
    VCommandBuffer(VCommandBuffer&& other);
    ~VCommandBuffer();

    void operator=(VCommandBuffer&& other);

    VkCommandBuffer impl=nullptr;

    void reset();

    void begin(Usage usageFlags);
    void begin();
    void end();

    void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                         AbstractGraphicsApi::Pass*  p,
                         uint32_t width,uint32_t height);
    void endRenderPass();

    void clear(AbstractGraphicsApi::Image& img, float r, float g, float b, float a);
    void setPipeline(AbstractGraphicsApi::Pipeline& p);
    void setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u);

    void draw(size_t size);
    void setVbo(const AbstractGraphicsApi::Buffer& b);

    void copy(Detail::VBuffer&  dest, size_t offsetDest, const Detail::VBuffer& src, size_t offsetSrc, size_t size);
    void copy(Detail::VTexture& dest, size_t width, size_t height, const Detail::VBuffer& src);

    void changeLayout(Detail::VTexture& dest, VkImageLayout oldLayout, VkImageLayout newLayout);

  private:
    VkDevice        device=nullptr;
    VkCommandPool   pool  =VK_NULL_HANDLE;
  };

}}
