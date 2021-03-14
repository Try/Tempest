#include "vuniformslay.h"

#include <Tempest/UniformsLayout>
#include "vdevice.h"
#include "gapi/shaderreflection.h"

using namespace Tempest;
using namespace Tempest::Detail;

VUniformsLay::VUniformsLay(VDevice& dev, const std::vector<UniformsLayout::Binding>& comp)
  : dev(dev.device) {
  ShaderReflection::merge(lay, pb, comp);
  adjustSsboBindings();

  if(lay.size()<=32) {
    VkDescriptorSetLayoutBinding bind[32]={};
    implCreate(bind);
    } else {
    std::unique_ptr<VkDescriptorSetLayoutBinding[]> bind(new VkDescriptorSetLayoutBinding[lay.size()]);
    implCreate(bind.get());
    }
  }

VUniformsLay::VUniformsLay(VDevice& dev, const std::vector<UniformsLayout::Binding>* sh[], size_t cnt)
  : dev(dev.device) {
  ShaderReflection::merge(lay, pb, sh, cnt);
  adjustSsboBindings();

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
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    };

  uint32_t count = 0;
  for(size_t i=0;i<lay.size();++i){
    auto& b=bind[count];
    auto& e=lay[i];

    if(e.stage==UniformsLayout::Stage(0))
      continue;

    b.binding         = e.layout;
    b.descriptorCount = 1;
    b.descriptorType  = types[e.cls];

    b.stageFlags      = 0;
    if(e.stage&UniformsLayout::Compute)
      b.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if(e.stage&UniformsLayout::Vertex)
      b.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    if(e.stage&UniformsLayout::Control)
      b.stageFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if(e.stage&UniformsLayout::Evaluate)
      b.stageFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if(e.stage&UniformsLayout::Geometry)
      b.stageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if(e.stage&UniformsLayout::Fragment)
      b.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    ++count;
    }

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount = count;
  info.pBindings    = bind;

  vkAssert(vkCreateDescriptorSetLayout(dev,&info,nullptr,&impl));
  }

void VUniformsLay::adjustSsboBindings() {
  for(auto& i:lay)
    if(i.size==0)
      i.size = VK_WHOLE_SIZE;
  for(auto& i:lay)
    if(i.cls==UniformsLayout::SsboR  ||
       i.cls==UniformsLayout::SsboRW ||
       i.cls==UniformsLayout::ImgR   ||
       i.cls==UniformsLayout::ImgRW ) {
      hasSSBO = true;
      }
  }

