#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Color>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDevice;
class VSwapchain;

class VRenderPass : public AbstractGraphicsApi::Pass {
  public:
    VRenderPass()=default;
    VRenderPass(VDevice &device,VkFormat format,
                FboMode in,const Color* clear,
                FboMode out,const float zclear);
    VRenderPass(VRenderPass&& other);
    ~VRenderPass();

    void operator=(VRenderPass&& other);

    VkRenderPass   renderPass=VK_NULL_HANDLE;
    Tempest::Color color;

  private:
    VkDevice     device=nullptr;
  };

}}
