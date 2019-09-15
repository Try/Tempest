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
    VRenderPass()=default;
    VRenderPass(VDevice& device, const Attachment** attach, size_t attCount);
    VRenderPass(VRenderPass&& other);
    ~VRenderPass();

    void operator=(VRenderPass&& other);

    struct Inst {
      VkRenderPass   impl=VK_NULL_HANDLE;
      };
    Inst&                         instance();

    Tempest::Color                color;
    float                         zclear=1.0f;

    uint8_t                       attachCount=1;

  private:
    VkDevice                      device=nullptr;
    std::vector<Inst>             inst;
    std::unique_ptr<Attachment[]> att;

    VkRenderPass                  createInstance();
  };

}}
