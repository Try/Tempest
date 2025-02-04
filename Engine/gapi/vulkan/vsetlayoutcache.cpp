#if defined(TEMPEST_BUILD_VULKAN)

#include "vsetlayoutcache.h"
#include "gapi/vulkan/vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VSetLayoutCache::VSetLayoutCache(VDevice& dev)
  :dev(dev) {
  }

VSetLayoutCache::~VSetLayoutCache() {
  for(auto& i:layouts)
    vkDestroyDescriptorSetLayout(dev.device.impl, i.lay, nullptr);
  }

VSetLayoutCache::Layout VSetLayoutCache::findLayout(const LayoutDesc &l) {
  std::lock_guard<std::mutex> guard(syncLay);
  for(auto& i:layouts) {
    if(i.desc!=l)
      continue;
    return i;
    }

  Layout& ret = layouts.emplace_back();
  ret.desc = l;

  VkDescriptorSetLayoutBinding bind[MaxBindings] = {};
  VkDescriptorBindingFlags     flg [MaxBindings] = {};
  uint32_t                     count             = 0;
  for(size_t i=0; i<MaxBindings; ++i) {
    if(l.bindings[i]==ShaderReflection::Count)
      continue;

    auto& b = bind[count];
    if((l.runtime & (1u << i))!=0)
      flg[count] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    b.binding         = uint32_t(i);
    b.descriptorCount = l.count[i];
    b.descriptorCount = std::max<uint32_t>(1, b.descriptorCount); // WA for VUID-VkGraphicsPipelineCreateInfo-layout-07988
    b.descriptorType  = nativeFormat(l.bindings[i]);
    b.stageFlags      = nativeFormat(l.stage[i]);
    ++count;
    }

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags = {};
  bindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlags.bindingCount  = count;
  bindingFlags.pBindingFlags = flg;

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext        = nullptr;
  info.flags        = 0;
  info.bindingCount = count;
  info.pBindings    = bind;

  if(l.isUpdateAfterBind()) {
    info.pNext  = &bindingFlags;
    info.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    }

  try {
    vkAssert(vkCreateDescriptorSetLayout(dev.device.impl, &info, nullptr, &ret.lay));
    }
  catch(...) {
    layouts.pop_back();
    throw;
    }
  return ret;
  }

#endif
