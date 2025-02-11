#if defined(TEMPEST_BUILD_VULKAN)

#include "vpsolayoutcache.h"
#include "gapi/vulkan/vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VPsoLayoutCache::VPsoLayoutCache(VDevice& dev)
  : dev(dev) {
  }

VPsoLayoutCache::~VPsoLayoutCache() {
  for(auto& i:layouts)
    vkDestroyPipelineLayout(dev.device.impl, i.pLay, nullptr);
  }

VkPipelineLayout VPsoLayoutCache::findLayout(const ShaderReflection::PushBlock& pb, VkDescriptorSetLayout lay) {
  std::lock_guard<std::mutex> guard(sync);

  const auto stageFlags = nativeFormat(pb.stage);
  for(auto& i:layouts)
    if(i.lay==lay && i.pushStage==stageFlags && i.pushSize==pb.size)
      return i.pLay;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.pSetLayouts            = &lay;
  pipelineLayoutInfo.setLayoutCount         = 1;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  VkPushConstantRange push = {};
  if(pb.size>0) {
    push.stageFlags = nativeFormat(pb.stage);
    push.offset     = 0;
    push.size       = uint32_t(pb.size);

    pipelineLayoutInfo.pPushConstantRanges    = &push;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

  auto& ret = layouts.emplace_back();
  ret.lay       = lay;
  ret.pushStage = stageFlags;
  ret.pushSize  = uint32_t(pb.size);
  try {
    vkAssert(vkCreatePipelineLayout(dev.device.impl, &pipelineLayoutInfo, nullptr, &ret.pLay));
    }
  catch(...) {
    layouts.pop_back();
    throw;
    }
  return ret.pLay;
  }

VkPipelineLayout VPsoLayoutCache::findLayout(const ShaderReflection::PushBlock& pb, const ShaderReflection::LayoutDesc& lay) {
  auto dLay = dev.setLayouts.findLayout(lay);
  return dev.psoLayouts.findLayout(pb, dLay);
  }

#endif

