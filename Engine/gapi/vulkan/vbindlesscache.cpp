#if defined(TEMPEST_BUILD_VULKAN)

#include "vbindlesscache.h"

#include <bit>

#include "gapi/vulkan/vdescriptorarray.h"
#include "gapi/vulkan/vdevice.h"
#include "gapi/vulkan/vpipelinelay.h"
#include "gapi/vulkan/vpipeline.h"
#include "gapi/vulkan/vtexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

bool VBindlessCache::Bindings::operator ==(const Bindings &other) const {
  for(size_t i=0; i<MaxBindings; ++i) {
    if(data[i]!=other.data[i])
      return false;
    if(smp[i]!=other.smp[i])
      return false;
    if(offset[i]!=other.offset[i])
      return false;
    }
  return array==other.array;
  }

bool VBindlessCache::Bindings::operator !=(const Bindings &other) const {
  return !(*this==other);
  }

bool VBindlessCache::Bindings::contains(const AbstractGraphicsApi::NoCopy* res) const {
  for(size_t i=0; i<MaxBindings; ++i)
    if(data[i]==res)
      return true;
  return false;
  }


VBindlessCache::VBindlessCache(VDevice& dev)
  : dev(dev) {
  }

VBindlessCache::~VBindlessCache() {
  for(auto& i:descriptors) {
    (void)i;
    // should be deleted already
    // vkDestroyDescriptorPool(dev.device.impl, i.pool, nullptr);
    }

  for(auto& i:pLayouts)
    vkDestroyPipelineLayout(dev.device.impl, i.pLay, nullptr);
  }

void VBindlessCache::notifyDestroy(const AbstractGraphicsApi::NoCopy *res) {
  std::lock_guard<std::mutex> guard(syncDesc);

  for(size_t i=0; i<descriptors.size();) {
    auto& d = descriptors[i];
    if(!d.bindings.contains(res)) {
      ++i;
      continue;
      }
    vkDestroyDescriptorPool(dev.device.impl, d.pool, nullptr);
    d = std::move(descriptors.back());
    descriptors.pop_back();
    }
  }

VBindlessCache::Inst VBindlessCache::inst(const PushBlock& pb, const LayoutDesc& layout, const Bindings &binding) {
  auto lx = layout;
  for(uint32_t mask = lx.runtime; mask!=0;) {
    const int i = std::countr_zero(mask);
    mask ^= (1u << i);
    auto* a = reinterpret_cast<const VDescriptorArray2*>(binding.data[i]);
    lx.count[i] = uint32_t(a->size());
    }

  auto lt = dev.setLayouts.findLayout(lx);
  auto px = findPsoLayout(pb, lt.lay);

  Inst ret;
  ret.lay  = lt.lay;
  ret.pLay = px.pLay;

  std::lock_guard<std::mutex> guard(syncDesc);
  for(auto& i:descriptors) {
    if(i.lay!=lt.lay || i.bindings!=binding)
      continue;
    ret.set = i.set;
    return ret;
    }

  auto& desc = descriptors.emplace_back();
  try {
    desc.lay      = lt.lay;
    desc.bindings = binding;
    desc.pool     = allocPool(lx);
    desc.set      = allocDescSet(desc.pool, lt.lay);
    initDescriptorSet(desc.set, binding, lx);
    }
  catch(...) {
    if(desc.pool!=VK_NULL_HANDLE)
      vkDestroyDescriptorPool(dev.device.impl, desc.pool, nullptr);
    descriptors.pop_back();
    throw;
    }

  ret.set  = desc.set;
  ret.lay  = lt.lay;
  ret.pLay = px.pLay;
  return ret;
  }

VBindlessCache::PLayout VBindlessCache::findPsoLayout(const ShaderReflection::PushBlock& pb, VkDescriptorSetLayout lay) {
  std::lock_guard<std::mutex> guard(syncPLay);

  const auto stageFlags = nativeFormat(pb.stage);
  for(auto& i:pLayouts)
    if(i.lay==lay && i.pushStage==stageFlags && i.pushSize==pb.size)
      return i;

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

  auto& ret = pLayouts.emplace_back();
  ret.lay       = lay;
  ret.pushStage = stageFlags;
  ret.pushSize  = uint32_t(pb.size);
  try {
    vkAssert(vkCreatePipelineLayout(dev.device.impl, &pipelineLayoutInfo, nullptr, &ret.pLay));
    }
  catch(...) {
    pLayouts.pop_back();
    throw;
    }
  return ret;
  }

void VBindlessCache::addPoolSize(VkDescriptorPoolSize *p, size_t &sz, uint32_t cnt, VkDescriptorType elt) {
  for(size_t i=0; i<sz; ++i){
    if(p[i].type==elt) {
      p[i].descriptorCount += cnt;
      return;
      }
    }
  p[sz].type            = elt;
  p[sz].descriptorCount = cnt;
  sz++;
  }

VkDescriptorPool VBindlessCache::allocPool(const LayoutDesc &l) {
  VkDescriptorPoolSize poolSize[int(ShaderReflection::Class::Count)] = {};
  size_t               pSize   = 0;

  for(size_t i=0; i<MaxBindings; ++i) {
    if(l.stage[i]==Tempest::Detail::ShaderReflection::None)
      continue;
    auto     cls = l.bindings[i];
    uint32_t cnt = l.count[i];
    if((l.runtime & (1u << i))!=0)
      cnt = std::max<uint32_t>(1, l.count[i]);
    switch(cls) {
      case ShaderReflection::Ubo:     addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);             break;
      case ShaderReflection::Texture: addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);     break;
      case ShaderReflection::Image:   addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);              break;
      case ShaderReflection::Sampler: addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_SAMPLER);                    break;
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW:  addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);             break;
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW:   addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);              break;
      case ShaderReflection::Tlas:    addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR); break;
      case ShaderReflection::Push:    break;
      case ShaderReflection::Count:   break;
      }
    }

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = 1;
  poolInfo.flags         = 0;
  poolInfo.poolSizeCount = uint32_t(pSize);
  poolInfo.pPoolSizes    = poolSize;

  if(l.isUpdateAfterBind())
    poolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

  VkDevice dev = this->dev.device.impl;
  VkDescriptorPool ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorPool(dev,&poolInfo,nullptr,&ret));
  return ret;
  }

VkDescriptorSet VBindlessCache::allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay) {
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &lay;

  VkDevice        dev  = this->dev.device.impl;
  VkDescriptorSet desc = VK_NULL_HANDLE;
  VkResult        ret  = vkAllocateDescriptorSets(dev,&allocInfo,&desc);
  if(ret==VK_ERROR_FRAGMENTED_POOL)
    return VK_NULL_HANDLE;
  if(ret!=VK_SUCCESS)
    return VK_NULL_HANDLE;
  return desc;
  }

void VBindlessCache::initDescriptorSet(VkDescriptorSet dset, const Bindings &binding, const LayoutDesc& l) {
  VkCopyDescriptorSet    cpy[MaxBindings] = {};
  uint32_t               cntCpy = 0;

  WriteInfo              winfo[MaxBindings] = {};
  VkWriteDescriptorSet   wr   [MaxBindings] = {};
  uint32_t               cntWr = 0;

  for(size_t i=0; i<MaxBindings; ++i) {
    if((l.active & (1u << i))==0)
      continue;

    VkCopyDescriptorSet& cx = cpy[cntCpy];
    if((binding.array & (1u << i))!=0) {
      // assert((l.runtime & (1u << i))!=0);
      auto arr = reinterpret_cast<const VDescriptorArray2*>(binding.data[i]);
      cx.sType           = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
      cx.pNext           = nullptr;
      cx.srcSet          = arr->set();
      cx.srcBinding      = 0;
      cx.srcArrayElement = 0;
      cx.dstSet          = dset;
      cx.dstBinding      = uint32_t(i);
      cx.dstArrayElement = 0;
      cx.descriptorCount = uint32_t(arr->size());
      if(cx.descriptorCount>0)
        ++cntCpy;
      continue;
      }

    VPushDescriptor::write(dev, wr[cntWr], winfo[cntWr], uint32_t(i), l.bindings[i],
                           binding.data[i], binding.offset[i], binding.smp[i]);

    VkWriteDescriptorSet& wx = wr[cntWr];
    wx.dstSet = dset;
    if(wx.descriptorCount>0)
      ++cntWr;
    }

  vkUpdateDescriptorSets(dev.device.impl, cntWr, wr, cntCpy, cpy);
  }

#endif
