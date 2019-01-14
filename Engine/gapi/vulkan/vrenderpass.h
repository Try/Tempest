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
    VRenderPass(VDevice &device,
                VkFormat format, VkFormat zformat,
                FboMode color, const Color* clear,
                FboMode zbuf, const float *zclear);
    VRenderPass(VRenderPass&& other);
    ~VRenderPass();

    void operator=(VRenderPass&& other);

    VkRenderPass   renderPass=VK_NULL_HANDLE;
    Tempest::Color color;
    float          zclear=1.0f;

    uint8_t        attachCount=1;

  private:
    VkDevice       device=nullptr;
  };

}}
