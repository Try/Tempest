#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDevice;
class VSwapchain;

class VRenderPass : public AbstractGraphicsApi::Pass {
  public:
    VRenderPass()=default;
    VRenderPass(VDevice &device,VkFormat format);
    VRenderPass(VRenderPass&& other);
    ~VRenderPass();

    void operator=(VRenderPass&& other);

    VkRenderPass renderPass=VK_NULL_HANDLE;

  private:
    VkDevice     device=nullptr;
  };

}}
