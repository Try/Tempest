#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Color>
#include <vulkan/vulkan.hpp>

namespace Tempest {

class Attachment;

namespace Detail {

class VDevice;
class VSwapchain;

class VRenderPass : public AbstractGraphicsApi::Pass {
  public:
    struct Att {
      VkFormat frm=VK_FORMAT_UNDEFINED;
      };

    VRenderPass()=default;
    VRenderPass(VDevice& device, VSwapchain& sw, const Attachment** attach, uint8_t attCount);
    VRenderPass(VDevice& device, VSwapchain& sw, const VkFormat*    attach, uint8_t attCount);
    VRenderPass(VRenderPass&& other);
    ~VRenderPass();

    void operator=(VRenderPass&& other);

    Tempest::Color                color;
    float                         zclear=1.0f;
    uint8_t                       attCount=0;
    VkRenderPass                  impl=VK_NULL_HANDLE;

  private:
    VkDevice                      device=nullptr;
    VkRenderPass                  createInstance(VSwapchain& sw, const Attachment **attach, uint8_t attCount);
    VkRenderPass                  createLayoutInstance(VSwapchain& sw, const VkFormat    *attach, uint8_t attCount);
  };

}}
