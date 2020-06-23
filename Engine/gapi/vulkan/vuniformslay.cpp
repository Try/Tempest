#include "vuniformslay.h"

#include <Tempest/UniformsLayout>
#include "vdevice.h"
#include "gapi/shaderreflection.h"

using namespace Tempest;
using namespace Tempest::Detail;

VUniformsLay::VUniformsLay(VkDevice dev,
                           const std::vector<UniformsLayout::Binding>& vs,
                           const std::vector<UniformsLayout::Binding>& fs)
  : dev(dev) {
  ShaderReflection::merge(lay, pb, vs,fs);
  if(lay.size()<=32) {
    VkDescriptorSetLayoutBinding bind[32]={};
    implCreate(bind);
    } else {
    std::unique_ptr<VkDescriptorSetLayoutBinding[]> bind(new VkDescriptorSetLayoutBinding[lay.size()]);
    implCreate(bind.get());
    }
  }

VUniformsLay::~VUniformsLay() {
  for(auto& i:pool)
    vkDestroyDescriptorPool(dev,i.impl,nullptr);
  vkDestroyDescriptorSetLayout(dev,impl,nullptr);
  }

void VUniformsLay::implCreate(VkDescriptorSetLayoutBinding* bind) {
  static const VkDescriptorType types[] = {
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    };

  for(size_t i=0;i<lay.size();++i){
    auto& b=bind[i];
    auto& e=lay[i];

    b.binding         = e.layout;
    b.descriptorCount = 1;
    b.descriptorType  = types[e.cls];

    b.stageFlags      = 0;
    if(e.stage&UniformsLayout::Vertex)
      b.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    if(e.stage&UniformsLayout::Fragment)
      b.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount = uint32_t(lay.size());
  info.pBindings    = bind;

  vkAssert(vkCreateDescriptorSetLayout(dev,&info,nullptr,&impl));
  }

