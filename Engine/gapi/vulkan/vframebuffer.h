#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDevice;
class VRenderPass;
class VSwapchain;
class VTexture;
class VFramebufferLayout;

class VFramebuffer : public AbstractGraphicsApi::Fbo {
  public:
    VFramebuffer(VDevice &device, VFramebufferLayout& lay, VSwapchain &swapchain,  size_t image);
    VFramebuffer(VDevice &device, VFramebufferLayout& lay, VSwapchain &swapchain,  size_t image, VTexture& zbuf);
    VFramebuffer(VDevice &device, VFramebufferLayout& lay, uint32_t w, uint32_t h, VTexture& color, VTexture& zbuf);
    VFramebuffer(VDevice &device, VFramebufferLayout& lay, uint32_t w, uint32_t h, VTexture& color);
    VFramebuffer(VFramebuffer&& other);
    ~VFramebuffer();

    void operator=(VFramebuffer&& other);

    VkFramebuffer                           impl=VK_NULL_HANDLE;

  private:
    VkDevice                                device=nullptr;
    Detail::DSharedPtr<VFramebufferLayout*> rp;

    VkFramebuffer allocFbo(uint32_t w, uint32_t h,const VkImageView* attach,size_t cnt);
  };

}}
