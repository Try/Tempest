#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VShader;
class VDevice;
class VRenderPass;

class VPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    VPipeline();
    VPipeline(VDevice &device, VRenderPass &pass, uint32_t width, uint32_t height,
              VShader &vert, VShader &frag);
    VPipeline(VPipeline&& other);
    ~VPipeline();

    void operator=(VPipeline&& other);

    VkPipeline       graphicsPipeline=VK_NULL_HANDLE;

  private:
    VkDevice         device        =nullptr;
    VkPipelineLayout pipelineLayout=VK_NULL_HANDLE;
  };

}}
