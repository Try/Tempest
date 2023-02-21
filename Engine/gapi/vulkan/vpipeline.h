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
    VPipeline(VDevice &device, const RenderState &st, Topology tp, const VPipelineLay& ulay,
              const VShader** sh, size_t count);
    VPipeline(VPipeline&& other) = delete;
    ~VPipeline();

    struct Inst {
      Inst(VkPipeline val, size_t stride):val(val),stride(stride){}
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      VkPipeline val;
      size_t     stride;
      };

    VkPipelineLayout   pipelineLayout = VK_NULL_HANDLE;
    VkShaderStageFlags pushStageFlags = 0;
    uint32_t           pushSize       = 0;
    uint32_t           defaultStride  = 0;

    VkPipeline         instance(const std::shared_ptr<VFramebufferMap::RenderPass>& lay, size_t stride);
    VkPipeline         instance(const VkPipelineRenderingCreateInfoKHR& info, size_t stride);

    IVec3              workGroupSize() const override;

    VkPipeline         meshPipeline() const;
    VkPipelineLayout   meshPipelineLayout() const;

  private:
    struct InstRp : Inst {
      InstRp(const std::shared_ptr<VFramebufferMap::RenderPass>& lay, size_t stride, VkPipeline val):Inst(val,stride),lay(lay){}
      std::shared_ptr<VFramebufferMap::RenderPass> lay;
      };

    struct InstDr : Inst {
      InstDr(const VkPipelineRenderingCreateInfoKHR& lay, size_t stride, VkPipeline val):Inst(val,stride),lay(lay){
        std::memcpy(colorFrm, lay.pColorAttachmentFormats, lay.colorAttachmentCount*sizeof(VkFormat));
        }
      VkPipelineRenderingCreateInfoKHR lay;
      VkFormat                         colorFrm[MaxFramebufferAttachments] = {};

      bool                             isCompatible(const VkPipelineRenderingCreateInfoKHR& dr, size_t stride) const;
      };

    VkDevice                               device=nullptr;
    Tempest::RenderState                   st;
    size_t                                 declSize=0;
    DSharedPtr<const VShader*>             modules[5] = {};
    std::unique_ptr<Decl::ComponentType[]> decl;
    Topology                               tp = Topology::Triangles;
    IVec3                                  wgSize = {};

    VkPipelineLayout                       pipelineLayoutMs = VK_NULL_HANDLE;
    VkPipeline                             meshCompuePipeline = VK_NULL_HANDLE;

    std::vector<InstRp>                    instRp;
    std::vector<InstDr>                    instDr;
    SpinLock                               sync;

    const VShader*                         findShader(ShaderReflection::Stage sh) const;

    void cleanup();
    static VkPipelineLayout      initLayout(VDevice& device, const VPipelineLay& uboLay, VkShaderStageFlags pushFlg, uint32_t& pushSize, bool isMeshCompPass);
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
    VCompPipeline(VDevice &device, const VPipelineLay& ulay, const VShader& comp);
    VCompPipeline(VCompPipeline&& other) = delete;
    ~VCompPipeline();

    IVec3              workGroupSize() const;

    VkDevice           device         = nullptr;
    VkPipelineLayout   pipelineLayout = VK_NULL_HANDLE;
    VkPipeline         impl           = VK_NULL_HANDLE;
    IVec3              wgSize;
    uint32_t           pushSize       = 0;
  };
}}
