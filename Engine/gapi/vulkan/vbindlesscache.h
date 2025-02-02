#pragma once

#include "gapi/abstractgraphicsapi.h"
#include "gapi/shaderreflection.h"

#include "vulkan_sdk.h"

#include <mutex>

namespace Tempest {
namespace Detail {

class VDevice;
class VPipelineLay;
class VCompPipeline;

class VBindlessCache {
  public:
    VBindlessCache(VDevice& dev);
    ~VBindlessCache();

    struct Bindings {
      AbstractGraphicsApi::NoCopy* data  [MaxBindings] = {};
      Sampler                      smp   [MaxBindings] = {};
      uint32_t                     offset[MaxBindings] = {}; // ssbo offset or texture mip-level
      uint32_t                     array               = {};

      bool operator == (const Bindings& other) const;
      bool operator != (const Bindings& other) const;

      bool contains(const AbstractGraphicsApi::NoCopy* res) const;
      };

    struct Inst {
      VkDescriptorSet       set  = VK_NULL_HANDLE;
      VkDescriptorSetLayout lay  = VK_NULL_HANDLE;
      VkPipelineLayout      pLay = VK_NULL_HANDLE;
      };

    Inst inst(const VCompPipeline &pso, const Bindings& binding);

    void notifyDestroy(const AbstractGraphicsApi::NoCopy* res);

  private:
    struct LayoutDesc {
      ShaderReflection::Class bindings[MaxBindings] = {};
      ShaderReflection::Stage stage   [MaxBindings] = {};
      uint32_t                count   [MaxBindings] = {};
      uint32_t                runtime = 0;
      uint32_t                array   = 0;

      bool operator == (const LayoutDesc& other) const;
      bool operator != (const LayoutDesc& other) const;

      bool isUpdateAfterBind() const;
      };

    struct Layout {
      LayoutDesc            desc;
      VkDescriptorSetLayout lay;
      };

    struct PLayout {
      VkShaderStageFlags    pushStage = 0;
      uint32_t              pushSize  = 0;
      VkDescriptorSetLayout lay       = VK_NULL_HANDLE;
      VkPipelineLayout      pLay      = VK_NULL_HANDLE;
      };

    struct DSet {
      Bindings         bindings;

      VkDescriptorPool pool = VK_NULL_HANDLE;
      VkDescriptorSet  set  = VK_NULL_HANDLE;
      };

    LayoutDesc       toLayout(const VPipelineLay& l, const Bindings &binding);
    Layout           findLayout(const LayoutDesc& l);

    PLayout          findPsoLayout(const VPipelineLay &pLay, VkDescriptorSetLayout lay);

    void             addPoolSize(VkDescriptorPoolSize *p, size_t &sz, uint32_t cnt, VkDescriptorType elt);
    VkDescriptorPool allocPool(const LayoutDesc &l);
    VkDescriptorSet  allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay);

    void             initDescriptorSet(VkDescriptorSet set, const Bindings& binding, const LayoutDesc &lx);

    VDevice&            dev;

    std::mutex           syncLay;
    std::vector<Layout>  layouts;

    std::mutex           syncPLay;
    std::vector<PLayout> pLayouts;

    std::mutex           syncDesc;
    std::vector<DSet>    descriptors;
  };

}
}
