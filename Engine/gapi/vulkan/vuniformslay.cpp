#include "vuniformslay.h"

#include <Tempest/UniformsLayout>
#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VUniformsLay::VUniformsLay(VkDevice dev, const Tempest::UniformsLayout &ulay)
  :dev(dev) {
  hint.resize(ulay.size());
  if(ulay.size()<=32) {
    VkDescriptorSetLayoutBinding bind[32]={};
    implCreate(ulay,bind);
    } else {
    std::unique_ptr<VkDescriptorSetLayoutBinding[]> bind(new VkDescriptorSetLayoutBinding[ulay.size()]);
    implCreate(ulay,bind.get());
    }

  for(auto& i:hint)
    if(i==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
      offsetsCnt++;
  }

VUniformsLay::~VUniformsLay() {
  for(auto& i:pool)
    vkDestroyDescriptorPool(dev,i.impl,nullptr);
  vkDestroyDescriptorSetLayout(dev,impl,nullptr);
  }

void VUniformsLay::implCreate(const UniformsLayout &ulay,VkDescriptorSetLayoutBinding* bind) {
  static const VkDescriptorType types[] = {
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    };

  for(size_t i=0;i<ulay.size();++i){
    auto& b=bind[i];
    auto& e=ulay[i];

    b.binding         = e.layout;
    b.descriptorCount = 1;
    b.descriptorType  = types[e.cls];
    hint[i]           = types[e.cls];

    b.stageFlags      = 0;
    if(e.stage&UniformsLayout::Vertex)
      b.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    if(e.stage&UniformsLayout::Fragment)
      b.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount = uint32_t(ulay.size());
  info.pBindings    = bind;

  vkAssert(vkCreateDescriptorSetLayout(dev,&info,nullptr,&impl));
  }

