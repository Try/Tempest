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

class VCommandBundle : public AbstractGraphicsApi::CommandBundle {
  public:
    VCommandBundle(VkDevice& device, VkCommandPool& pool, VFramebufferLayout* fbo);
    VCommandBundle(VCommandBundle&& other);
    ~VCommandBundle();

    VCommandBundle& operator = (VCommandBundle&& other);

    void reset();

    void begin();
    void end();
    bool isRecording() const;

    void setPipeline(AbstractGraphicsApi::Pipeline& p, uint32_t w, uint32_t h);
    void setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u, size_t offc, const uint32_t* offv);
    void setViewport(const Rect& r);

    void setVbo(const AbstractGraphicsApi::Buffer& b);
    void setIbo(const AbstractGraphicsApi::Buffer* b,Detail::IndexClass cls);

    void draw(size_t offset, size_t size);
    void drawIndexed(size_t ioffset, size_t isize, size_t voffset);

    VkDevice        device=nullptr;
    VkCommandPool   pool  =VK_NULL_HANDLE;
    VkCommandBuffer impl  =nullptr;
    bool            recording=false;

    Detail::DSharedPtr<VFramebufferLayout*> fboLay;

  private:
    void            implSetUniforms(VkCommandBuffer cmd, VPipeline& p, VDescriptorArray& u,
                                    size_t offc, const uint32_t* offv, uint32_t* buf);
  };
}
}

