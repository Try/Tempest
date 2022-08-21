#if defined(TEMPEST_BUILD_VULKAN)

#include "vpipelinelay.h"

#include <Tempest/PipelineLayout>
#include "vdevice.h"
#include "gapi/shaderreflection.h"
#include "utility/smallarray.h"

using namespace Tempest;
using namespace Tempest::Detail;

VPipelineLay::VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh)
  : VPipelineLay(dev,&sh,1) {
  }

VPipelineLay::VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt)
  : dev(dev) {
  ShaderReflection::merge(lay, pb, sh, cnt);
  adjustSsboBindings();

  bool needMsHelper = false;
  for(size_t i=0; i<cnt; ++i) {
    if(sh[i]==nullptr)
      continue;
    for(auto& r:*sh[i])
      if(r.stage==ShaderReflection::Mesh) {
        needMsHelper = true;
        break;
        }
    }

  impl = create(MAX_BINDLESS);
  if(needMsHelper) {
    try {
      msHelper = createMsHelper();
      }
    catch(...) {
      vkDestroyDescriptorSetLayout(dev.device.impl,impl,nullptr);
      throw;
      }
    }
  }

VPipelineLay::~VPipelineLay() {
  for(auto& i:pool)
    vkDestroyDescriptorPool(dev.device.impl,i.impl,nullptr);
  if(msHelper!=VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(dev.device.impl,msHelper,nullptr);
  vkDestroyDescriptorSetLayout(dev.device.impl,impl,nullptr);
  }

size_t VPipelineLay::descriptorsCount() {
  return lay.size();
  }

VkDescriptorSetLayout VPipelineLay::create(uint32_t runtimeArraySz) const {
  SmallArray<VkDescriptorSetLayoutBinding,32> bind(lay.size());
  SmallArray<VkDescriptorBindingFlags,32>     flg (lay.size());

  uint32_t count = 0;
  for(size_t i=0;i<lay.size();++i) {
    auto& b = bind[count];
    auto& e = lay[i];

    if(e.stage==ShaderReflection::Stage(0))
      continue;

    if(e.runtimeSized)
      flg[count] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT; else
      flg[count] = 0;
    b.binding         = e.layout;
    b.descriptorCount = e.runtimeSized ? runtimeArraySz : e.arraySize;
    b.descriptorType  = nativeFormat(e.cls);
    b.stageFlags      = nativeFormat(e.stage);
    if((b.stageFlags&VK_SHADER_STAGE_MESH_BIT_NV)==VK_SHADER_STAGE_MESH_BIT_NV && dev.props.meshlets.meshShaderEmulated) {
      b.stageFlags &= ~VK_SHADER_STAGE_MESH_BIT_NV;
      b.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
      b.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
      }
    ++count;
    }

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags = {};
  bindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlags.bindingCount  = count;
  bindingFlags.pBindingFlags = flg.get();

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext        = nullptr;
  info.flags        = 0;
  info.bindingCount = count;
  info.pBindings    = bind.get();

  if(runtimeSized)
    info.pNext = &bindingFlags;

  VkDescriptorSetLayout ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorSetLayout(dev.device.impl,&info,nullptr,&ret));
  return ret;
  }

VkDescriptorSetLayout VPipelineLay::createMsHelper() const {
  VkDescriptorSetLayoutBinding bind[1] = {};
  bind[0].binding         = 0;
  bind[0].descriptorCount = 1;
  bind[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bind[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext        = nullptr;
  info.flags        = 0;
  info.bindingCount = 1; // one for vertex
  info.pBindings    = bind;

  VkDescriptorSetLayout ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorSetLayout(dev.device.impl,&info,nullptr,&ret));
  return ret;
  }

void VPipelineLay::adjustSsboBindings() {
  for(auto& i:lay) {
    if(i.byteSize==0) {
      i.byteSize = VK_WHOLE_SIZE;
      }
    if(i.runtimeSized) {
      runtimeSized = true;
      }
    }
  }

#endif

