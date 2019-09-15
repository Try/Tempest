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
    VFramebuffer(VDevice &device, VSwapchain &swapchain,  size_t image);
    VFramebuffer(VDevice &device, VSwapchain &swapchain,  size_t image, VTexture& zbuf);
    VFramebuffer(VDevice &device, uint32_t w, uint32_t h, VTexture& color, VTexture& zbuf);
    VFramebuffer(VDevice &device, uint32_t w, uint32_t h, VTexture& color);
    VFramebuffer(VFramebuffer&& other);
    ~VFramebuffer();

    void operator=(VFramebuffer&& other);

    struct Inst final {
      Inst(VRenderPass* rp,VkFramebuffer val):rp(rp),impl(val){}
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      Detail::DSharedPtr<VRenderPass*> rp;
      VkFramebuffer                    impl=VK_NULL_HANDLE;
      };
    std::vector<Inst> inst;
    Inst&             instance(VRenderPass &pass);

  private:
    VkDevice      device=nullptr;
    struct Inputs {
      Inputs(VSwapchain &swapchain,  size_t image);
      Inputs(VSwapchain &swapchain,  size_t image, VTexture& zbuf);
      Inputs(uint32_t w, uint32_t h, VTexture& color);
      Inputs(uint32_t w, uint32_t h, VTexture& color, VTexture& zbuf);

      VkFramebuffer alloc(VkDevice device, VkRenderPass rp);

      VkFramebufferCreateInfo         crt={};
      //Detail::DSharedPtr<VSwapchain*> sw;
      VSwapchain*                     sw=nullptr;
      size_t                          swImg=0;
      Detail::DSharedPtr<VTexture*>   tx[2];
      };
    Inputs        inputs;
  };

}}
