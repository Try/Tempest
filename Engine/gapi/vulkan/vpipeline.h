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
class VPipelineLay;

class VPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    VPipeline();
    VPipeline(VDevice &device,
              const RenderState &st, size_t stride, Topology tp, const VPipelineLay& ulayImpl,
              const VShader* vert, const VShader* ctrl, const VShader* tess, const VShader* geom,  const VShader* frag);
    VPipeline(VPipeline&& other);
    ~VPipeline();

    struct Inst final {
      Inst(VFramebufferLayout* lay,VkPipeline val):lay(lay),val(val){}
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      Detail::DSharedPtr<VFramebufferLayout*> lay;
      VkPipeline                              val;
      };

    VkPipelineLayout   pipelineLayout = VK_NULL_HANDLE;
    VkShaderStageFlags pushStageFlags = 0;
    bool               ssboBarriers   = false;

    Inst&             instance(VFramebufferLayout &lay);

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
    static VkPipelineLayout      initLayout(VkDevice device, const VPipelineLay& uboLay, VkShaderStageFlags& pushFlg);
    static VkPipeline            initGraphicsPipeline(VkDevice device, VkPipelineLayout layout,
                                                      const VFramebufferLayout &lay, const RenderState &st,
                                                      const Decl::ComponentType *decl, size_t declSize, size_t stride,
                                                      Topology tp,
                                                      const DSharedPtr<const VShader*>* shaders);
  friend class VCompPipeline;
  };

class VCompPipeline : public AbstractGraphicsApi::CompPipeline {
  public:
    VCompPipeline();
    VCompPipeline(VDevice &device, const VPipelineLay& ulay, VShader &comp);
    VCompPipeline(VCompPipeline&& other);
    ~VCompPipeline();

    VkDevice           device         = nullptr;
    VkPipelineLayout   pipelineLayout = VK_NULL_HANDLE;
    VkPipeline         impl           = VK_NULL_HANDLE;
    bool               ssboBarriers   = false;
  };
}}
