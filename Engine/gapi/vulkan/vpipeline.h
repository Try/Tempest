#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
#include <vector>
#include "vulkan_sdk.h"

#include "gapi/vulkan/vframebuffermap.h"
#include "gapi/vulkan/vshader.h"
#include "gapi/shaderreflection.h"
#include "utility/dptr.h"
#include "utility/spinlock.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    VPipeline();
    VPipeline(VDevice &device, Topology tp, const RenderState &st, const VShader** sh, size_t count);
    VPipeline(VPipeline&& other) = delete;
    ~VPipeline();

    struct Inst {
      Inst(VkPipeline val, VkPipelineLayout pLay, size_t stride):val(val),pLay(pLay),stride(stride){}
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      VkPipeline       val;
      VkPipelineLayout pLay = VK_NULL_HANDLE;
      size_t           stride;
      };

    using Binding    = ShaderReflection::Binding;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    using SyncDesc   = ShaderReflection::SyncDesc;

    PushBlock             pb;
    LayoutDesc            layout;
    SyncDesc              sync;

    VkPipelineLayout      pipelineLayout = VK_NULL_HANDLE;
    uint32_t              defaultStride  = 0;

    VkPipeline         instance(const std::shared_ptr<VFramebufferMap::RenderPass>& lay, VkPipelineLayout pLay, size_t stride);
    VkPipeline         instance(const VkPipelineRenderingCreateInfoKHR& info, VkPipelineLayout pLay, size_t stride);

    IVec3              workGroupSize() const override;
    size_t             sizeofBuffer(size_t id, size_t arraylen) const override;

  private:
    struct InstRp : Inst {
      InstRp(const std::shared_ptr<VFramebufferMap::RenderPass>& lay, VkPipelineLayout pLay, size_t stride, VkPipeline val):Inst(val,pLay,stride),lay(lay){}
      std::shared_ptr<VFramebufferMap::RenderPass> lay;

      bool                             isCompatible(const std::shared_ptr<VFramebufferMap::RenderPass>& dr, VkPipelineLayout pLay, size_t stride) const;
      };

    struct InstDr : Inst {
      InstDr(const VkPipelineRenderingCreateInfoKHR& lay, VkPipelineLayout pLay, size_t stride, VkPipeline val):Inst(val,pLay,stride),lay(lay){
        std::memcpy(colorFrm, lay.pColorAttachmentFormats, lay.colorAttachmentCount*sizeof(VkFormat));
        }
      VkPipelineRenderingCreateInfoKHR lay;
      VkFormat                         colorFrm[MaxFramebufferAttachments] = {};

      bool                             isCompatible(const VkPipelineRenderingCreateInfoKHR& dr, VkPipelineLayout pLay, size_t stride) const;
      };

    VkDevice                               device = nullptr;
    Topology                               tp = Topology::Triangles;
    Tempest::RenderState                   st;
    size_t                                 declSize=0;
    DSharedPtr<const VShader*>             modules[5] = {};
    std::unique_ptr<Decl::ComponentType[]> decl;
    IVec3                                  wgSize = {};

    SpinLock                               syncInst;
    std::vector<InstRp>                    instRp;
    std::vector<InstDr>                    instDr;

    const VShader*                         findShader(ShaderReflection::Stage sh) const;
    void                                   cleanup();

    VkPipeline                   initGraphicsPipeline(VkDevice device, VkPipelineLayout layout,
                                                      const VFramebufferMap::RenderPass* rpLay, const VkPipelineRenderingCreateInfoKHR* dynLay, const RenderState &st,
                                                      const Decl::ComponentType *decl, size_t declSize, size_t stride,
                                                      Topology tp,
                                                      const DSharedPtr<const VShader*>* shaders);
  friend class VCompPipeline;
  };

class VCompPipeline : public AbstractGraphicsApi::CompPipeline {
  public:
    VCompPipeline(VDevice &device, const VShader& comp);
    VCompPipeline(VCompPipeline&& other) = delete;
    ~VCompPipeline();

    IVec3              workGroupSize() const override;
    size_t             sizeofBuffer(size_t id, size_t arraylen) const override;

    VkPipeline         instance(VkPipelineLayout pLay);

    using Binding    = ShaderReflection::Binding;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    using SyncDesc   = ShaderReflection::SyncDesc;

    PushBlock            pb;
    LayoutDesc           layout;
    SyncDesc             sync;

    VkPipelineLayout      pipelineLayout = VK_NULL_HANDLE;
    VkPipeline            impl           = VK_NULL_HANDLE;

  private:
    struct Inst {
      Inst(VkPipelineLayout dLay, VkPipeline val):val(val),dLay(dLay){}
      Inst(Inst&&)=default;
      Inst& operator = (Inst&&)=default;

      bool                  isCompatible(VkPipelineLayout dLay) const;

      VkPipeline            val;
      VkPipelineLayout      dLay;
      };

    VDevice&           device;
    IVec3              wgSize;

    Detail::DSharedPtr<const VShader*> shader;

    SpinLock           syncInst;
    std::vector<Inst>  inst;
  };
}}
