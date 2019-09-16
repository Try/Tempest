#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Color>
#include <vulkan/vulkan.hpp>

namespace Tempest {

class Attachment;

namespace Detail {

class VDevice;
class VSwapchain;
class VFramebufferLayout;

class VRenderPass : public AbstractGraphicsApi::Pass {
  public:
    struct Att {
      VkFormat frm=VK_FORMAT_UNDEFINED;
      };

    VRenderPass()=default;
    VRenderPass(VDevice& device, VSwapchain& sw, const Attachment** attach, uint8_t attCount);
    VRenderPass(VRenderPass&& other);
    ~VRenderPass();

    void operator=(VRenderPass&& other);

    struct Impl {
      Impl(VFramebufferLayout &lay,VkRenderPass impl):lay(&lay),impl(impl){}
      DSharedPtr<VFramebufferLayout*> lay;
      VkRenderPass                    impl=VK_NULL_HANDLE;
      };

    Impl&                         instance(VFramebufferLayout &lay);

    static VkRenderPass           createLayoutInstance(VkDevice &device, VSwapchain& sw, const VkFormat    *attach, uint8_t attCount);

    Tempest::Color                color;
    float                         zclear=1.0f;
    uint8_t                       attCount=0;

  private:
    VkDevice                      device=nullptr;
    VSwapchain*                   swapchain=nullptr;
    std::vector<Impl>             impl;
    std::unique_ptr<Attachment[]> input;

    static VkRenderPass           createInstance      (VkDevice &device, VSwapchain& sw, const Attachment *attach, const VkFormat *frm, uint8_t attCount);
  };

}}
