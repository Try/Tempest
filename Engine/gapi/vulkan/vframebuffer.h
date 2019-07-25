#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDevice;
class VRenderPass;
class VSwapchain;
class VTexture;

class VFramebuffer : public AbstractGraphicsApi::Fbo {
  public:
    VFramebuffer()=default;
    VFramebuffer(VDevice &device, VRenderPass &rp, VSwapchain &swapchain, size_t image);
    VFramebuffer(VDevice &device, VRenderPass &rp, VSwapchain &swapchain, size_t image, VTexture& zbuf);
    VFramebuffer(VDevice &device, VRenderPass &rp, uint32_t w, uint32_t h, VTexture& color, VTexture& texture);
    VFramebuffer(VDevice &device, VRenderPass &rp, uint32_t w, uint32_t h, VTexture& color);
    VFramebuffer(VFramebuffer&& other);
    ~VFramebuffer();

    void operator=(VFramebuffer&& other);

    VkFramebuffer impl=VK_NULL_HANDLE;

  private:
    VkDevice      device=nullptr;
  };

}}
