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
class VImage;

class VCommandBuffer:public AbstractGraphicsApi::CommandBuffer {
  public:
    VCommandBuffer()=delete;
    VCommandBuffer(VDevice &device,VCommandPool &pool);
    VCommandBuffer(VCommandBuffer&& other);
    ~VCommandBuffer();

    void operator=(VCommandBuffer&& other);

    VkCommandBuffer impl=nullptr;

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

  private:
    VkDevice        device=nullptr;
    VkCommandPool   pool  =VK_NULL_HANDLE;
  };

}}
