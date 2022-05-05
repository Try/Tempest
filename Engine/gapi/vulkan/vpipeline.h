#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
#include <vector>

#include "../utility/dptr.h"
#include "../utility/spinlock.h"
#include "gapi/shaderreflection.h"
#include "vframebuffermap.h"
#include "vshader.h"
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VPipelineLay;

class VPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    VPipeline();
    VPipeline(VDevice &device,
              const RenderState &st, size_t stride, Topology tp, const VPipelineLay& ulay,
              const VShader** sh, size_t count);
    VPipeline(VPipeline&& other) = delete;
    ~VPipeline();

    struct Inst {
      Inst(VkPipeline val):val(val){}
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      VkPipeline val;
      };

    VkPipelineLayout   pipelineLayout = VK_NULL_HANDLE;
    VkShaderStageFlags pushStageFlags = 0;
    uint32_t           pushSize       = 0;
    bool               ssboBarriers   = false;

    Inst&             instance(const std::shared_ptr<VFramebufferMap::RenderPass>& lay);
    Inst&             instance(const VkPipelineRenderingCreateInfoKHR& info);

  private:
    struct InstRp : Inst {
      InstRp(const std::shared_ptr<VFramebufferMap::RenderPass>& lay, VkPipeline val):Inst(val),lay(lay){}
      std::shared_ptr<VFramebufferMap::RenderPass> lay;
      };

    struct InstDr : Inst {
      InstDr(const VkPipelineRenderingCreateInfoKHR& lay, VkPipeline val):Inst(val),lay(lay){}
      VkPipelineRenderingCreateInfoKHR lay;
      bool                             isCompatible(const VkPipelineRenderingCreateInfoKHR& dr) const;
      };

    VkDevice                               device=nullptr;
    Tempest::RenderState                   st;
    size_t                                 declSize=0, stride=0;
    Topology                               tp = Topology::Triangles;
    DSharedPtr<const VShader*>             modules[5] = {};
    std::unique_ptr<Decl::ComponentType[]> decl;
    std::vector<InstRp>                    instRp;
    std::vector<InstDr>                    instDr;
    SpinLock                               sync;

    const VShader*                         findShader(ShaderReflection::Stage sh) const;

    void cleanup();
    static VkPipelineLayout      initLayout(VkDevice device, const VPipelineLay& uboLay, VkShaderStageFlags& pushFlg, uint32_t& pushSize);
    VkPipeline                   initGraphicsPipeline(VkDevice device, VkPipelineLayout layout,
                                                      const VFramebufferMap::RenderPass* rpLay, const VkPipelineRenderingCreateInfoKHR* dynLay, const RenderState &st,
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
    uint32_t           pushSize       = 0;
    bool               ssboBarriers   = false;
  };
}}
