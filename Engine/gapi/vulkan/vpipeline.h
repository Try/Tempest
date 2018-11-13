#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VShader;
class VDevice;
class VRenderPass;

class VPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    struct VUboLayout : AbstractGraphicsApi::UniformsLay {
      VUboLayout(VkDevice dev, const UniformsLayout& lay);
      VUboLayout(VkDevice dev, VkDescriptorSetLayout lay);
      ~VUboLayout();

      VkDevice              dev =nullptr;
      VkDescriptorSetLayout impl=VK_NULL_HANDLE;
      };

    VPipeline();
    VPipeline(VDevice &device, VRenderPass &pass,
              uint32_t width, uint32_t height,
              const Decl::ComponentType *decl, size_t declSize,
              size_t stride, Topology tp,
              const UniformsLayout& ulay,
              std::shared_ptr<AbstractGraphicsApi::UniformsLay> &ulayImpl,
              VShader &vert, VShader &frag);
    VPipeline(VPipeline&& other);
    ~VPipeline();

    void operator=(VPipeline&& other);

    VkPipeline            graphicsPipeline=VK_NULL_HANDLE;
    VkPipelineLayout      pipelineLayout  =VK_NULL_HANDLE;

  private:
    VkDevice              device        =nullptr;

    void cleanup();
    static VkPipelineLayout      initLayout(VkDevice device,VkDescriptorSetLayout uboLay);
    static VkDescriptorSetLayout initUboLayout(VkDevice device,const UniformsLayout &ulay);
    static VkPipeline initGraphicsPipeline(VkDevice device, VkPipelineLayout layout,
                                           VRenderPass& pass, uint32_t width, uint32_t height,
                                           const Decl::ComponentType *decl, size_t declSize, size_t stride,
                                           Topology tp,
                                           VShader &vert, VShader &frag);
  };

}}
