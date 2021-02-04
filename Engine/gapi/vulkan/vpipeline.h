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
    VPipeline(VDevice &device,
              const RenderState &st, size_t stride, Topology tp, const VUniformsLay& ulayImpl,
              const VShader* vert, const VShader* ctrl, const VShader* tess, const VShader* geom,  const VShader* frag);
    VPipeline(VPipeline&& other);
    ~VPipeline();

    struct Inst final {
      Inst(uint32_t w,uint32_t h,VFramebufferLayout* lay,VkPipeline val):w(w),h(h),lay(lay),val(val){}
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      uint32_t                                w=0;
      uint32_t                                h=0;
      Detail::DSharedPtr<VFramebufferLayout*> lay;
      VkPipeline                              val;
      };

    VkPipelineLayout   pipelineLayout = VK_NULL_HANDLE;
    VkShaderStageFlags pushStageFlags = 0;

    Inst&             instance(VFramebufferLayout &lay, uint32_t width, uint32_t height);

  private:
    VkDevice                               device=nullptr;
    Tempest::RenderState                   st;
    size_t                                 declSize=0, stride=0;
    Topology                               tp=Topology::Triangles;
    DSharedPtr<const VShader*>             modules[5];
    std::unique_ptr<Decl::ComponentType[]> decl;
    std::vector<Inst>                      inst;
    SpinLock                               sync;

    void cleanup();
    static VkPipelineLayout      initLayout(VkDevice device, const VUniformsLay& uboLay, VkShaderStageFlags& pushFlg);
    static VkPipeline            initGraphicsPipeline(VkDevice device, VkPipelineLayout layout,
                                                      const VFramebufferLayout &lay, const RenderState &st,
                                                      uint32_t width, uint32_t height,
                                                      const Decl::ComponentType *decl, size_t declSize, size_t stride,
                                                      Topology tp,
                                                      const DSharedPtr<const VShader*>* shaders);
  friend class VCompPipeline;
  };

class VCompPipeline : public AbstractGraphicsApi::CompPipeline {
  public:
    VCompPipeline();
    VCompPipeline(VDevice &device, const VUniformsLay& ulay, VShader &comp);
    VCompPipeline(VCompPipeline&& other);
    ~VCompPipeline();

    VkDevice           device         = nullptr;
    VkPipelineLayout   pipelineLayout = VK_NULL_HANDLE;
    VkPipeline         impl           = VK_NULL_HANDLE;
  };
}}
