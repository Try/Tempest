#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

#include <vulkan/vulkan.hpp>

#include <Tempest/RenderState>

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
    VPipeline(VDevice &device,
              const RenderState &st,
              const Decl::ComponentType *decl, size_t declSize,
              size_t stride, Topology tp,
              const UniformsLayout& ulay,
              std::shared_ptr<AbstractGraphicsApi::UniformsLay> &ulayImpl,
              VShader &vert, VShader &frag);
    VPipeline(VPipeline&& other);
    ~VPipeline();

    void operator=(VPipeline&& other);

    struct Inst final {
      Inst(uint32_t w,uint32_t h,VRenderPass* rp,VkPipeline val):w(w),h(h),rp(rp),val(val){}
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      uint32_t                         w=0;
      uint32_t                         h=0;
      Detail::DSharedPtr<VRenderPass*> rp;
      VkPipeline                       val;
      };

    std::vector<Inst> inst;
    VkPipelineLayout  pipelineLayout  =VK_NULL_HANDLE;

    Inst&             instance(VRenderPass &pass, uint32_t width, uint32_t height);

  private:
    VkDevice                               device=nullptr;
    Tempest::RenderState                   st;
    size_t                                 declSize=0, stride=0;
    Topology                               tp=Topology::Triangles;
    Detail::DSharedPtr<VShader*>           vs,fs;
    std::unique_ptr<Decl::ComponentType[]> decl;

    void cleanup();
    static VkPipelineLayout      initLayout(VkDevice device,VkDescriptorSetLayout uboLay);
    static VkDescriptorSetLayout initUboLayout(VkDevice device,const UniformsLayout &ulay);
    static VkPipeline            initGraphicsPipeline(VkDevice device, VkPipelineLayout layout,
                                                      VRenderPass& pass, const RenderState &st, uint32_t width, uint32_t height,
                                                      const Decl::ComponentType *decl, size_t declSize, size_t stride,
                                                      Topology tp,
                                                      VShader &vert, VShader &frag);
  };

}}
