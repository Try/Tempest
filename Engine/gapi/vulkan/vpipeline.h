#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
#include <vector>

#include "../utility/dptr.h"
#include "../utility/spinlock.h"
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VShader;
class VDevice;
class VFramebuffer;
class VFramebufferLayout;
class VUniformsLay;

class VPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    VPipeline();
    VPipeline(VDevice &device, const RenderState &st,
              const Decl::ComponentType *decl, size_t declSize,
              size_t stride, Topology tp,
              const VUniformsLay& ulayImpl,
              VShader &vert, VShader &frag);
    VPipeline(VPipeline&& other);
    ~VPipeline();

    void operator=(VPipeline&& other);

    struct Inst final {
      Inst(uint32_t w,uint32_t h,VFramebufferLayout* lay,VkPipeline val):w(w),h(h),lay(lay),val(val){}
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      uint32_t                                w=0;
      uint32_t                                h=0;
      Detail::DSharedPtr<VFramebufferLayout*> lay;
      VkPipeline                              val;
      };

    VkPipelineLayout  pipelineLayout  =VK_NULL_HANDLE;

    Inst&             instance(VFramebufferLayout &lay, uint32_t width, uint32_t height);

  private:
    VkDevice                               device=nullptr;
    Tempest::RenderState                   st;
    size_t                                 declSize=0, stride=0;
    Topology                               tp=Topology::Triangles;
    Detail::DSharedPtr<VShader*>           vs,fs;
    std::unique_ptr<Decl::ComponentType[]> decl;
    std::vector<Inst>                      inst;
    SpinLock                               sync;

    void cleanup();
    static VkPipelineLayout      initLayout(VkDevice device,VkDescriptorSetLayout uboLay);
    static VkPipeline            initGraphicsPipeline(VkDevice device, VkPipelineLayout layout,
                                                      const VFramebufferLayout &lay, const RenderState &st,
                                                      uint32_t width, uint32_t height,
                                                      const Decl::ComponentType *decl, size_t declSize, size_t stride,
                                                      Topology tp,
                                                      VShader &vert, VShader &frag);
  };

}}
