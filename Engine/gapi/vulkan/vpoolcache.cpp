#if defined(TEMPEST_BUILD_VULKAN)

#include "vpoolcache.h"
#include "vdevice.h"
#include "vdescriptorarray.h"

#include <bit>

using namespace Tempest;
using namespace Tempest::Detail;

static void addPoolSize(VkDescriptorPoolSize *p, size_t &sz, uint32_t cnt, VkDescriptorType elt) {
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


VPoolCache::VPoolCache(VDevice& dev)
  : dev(dev) {
  cache.reserve(MaxCache);
  }

VPoolCache::~VPoolCache() {
  for(auto& i:cache)
    vkDestroyDescriptorPool(dev.device.impl, i, nullptr);
  }

void VPoolCache::setupLimits() {
  if(dev.hasDeviceFeatures2) {
    VkPhysicalDeviceAccelerationStructurePropertiesKHR rtas = {};
    rtas.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    properties.pNext = &rtas;

    auto vkGetPhysicalDeviceProperties2 = PFN_vkGetPhysicalDeviceProperties2(vkGetInstanceProcAddr(dev.instance,"vkGetPhysicalDeviceProperties2KHR"));
    vkGetPhysicalDeviceProperties2(dev.physicalDevice, &properties);

    limits   = properties.properties.limits;
    rtLimits = rtas;
    } else {
    VkPhysicalDeviceProperties prop = {};
    vkGetPhysicalDeviceProperties(dev.physicalDevice, &prop);
    limits = prop.limits;
    }
  }

VkDescriptorPool VPoolCache::allocPool() {
  {
    std::lock_guard<std::mutex> guard(sync);
    if(cache.size()>0) {
      auto ret = cache.back();
      cache.pop_back();
      return ret;
      }
  }

  VkDescriptorPoolSize poolSize[int(ShaderReflection::Class::Count)] = {};
  size_t               pSize   = 0;

  const uint32_t maxResources = 8096;
  const uint32_t maxSamplers  = 1024;

  for(size_t i=0; i<ShaderReflection::Class::Count; ++i) {
    if(i==ShaderReflection::Push)
      continue;
    if(i==ShaderReflection::Tlas && !dev.props.raytracing.rayQuery)
      continue;
    auto& sz = poolSize[pSize];
    ++pSize;

    switch(ShaderReflection::Class(i)) {
      case ShaderReflection::Ubo: {
        sz.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sz.descriptorCount = std::min(limits.maxDescriptorSetUniformBuffers, maxResources);
        break;
        }
      case ShaderReflection::Texture: {
        sz.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sz.descriptorCount = std::min(limits.maxDescriptorSetSampledImages, maxResources);
        sz.descriptorCount = std::min(limits.maxDescriptorSetSamplers, sz.descriptorCount);
        sz.descriptorCount = std::min(maxSamplers, sz.descriptorCount);
        break;
        }
      case ShaderReflection::Image: {
        sz.type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sz.descriptorCount = std::min(limits.maxDescriptorSetSampledImages, maxResources);
        break;
        }
      case ShaderReflection::Sampler: {
        sz.type            = VK_DESCRIPTOR_TYPE_SAMPLER;
        sz.descriptorCount = std::min(limits.maxDescriptorSetSamplers, maxSamplers);
        break;
        }
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW: {
        sz.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sz.descriptorCount = std::min(limits.maxDescriptorSetStorageBuffers, maxResources);
        break;
        }
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW: {
        sz.type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        sz.descriptorCount = std::min(limits.maxDescriptorSetStorageImages, maxResources);
        break;
        }
      case ShaderReflection::Tlas: {
        if(dev.props.raytracing.rayQuery) {
          sz.type            = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
          sz.descriptorCount = std::min(rtLimits.maxDescriptorSetAccelerationStructures, maxSamplers); // note: can be less than 32
          }
        break;
        }
      case ShaderReflection::Push:    break;
      case ShaderReflection::Count:   break;
      }
    }

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = std::max(maxResources, maxSamplers);
  poolInfo.flags         = 0;
  poolInfo.poolSizeCount = uint32_t(pSize);
  poolInfo.pPoolSizes    = poolSize;

  VkDescriptorPool ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorPool(dev.device.impl, &poolInfo, nullptr, &ret));
  return ret;
  }

void VPoolCache::freePool(VkDescriptorPool p) {
  std::lock_guard<std::mutex> guard(sync);
  if(cache.size()>=MaxCache) {
    vkDestroyDescriptorPool(dev.device.impl, p, nullptr);
    return;
    }

  try {
    cache.push_back(p);
    vkResetDescriptorPool(dev.device.impl, p, 0);
    }
  catch(...) {
    vkDestroyDescriptorPool(dev.device.impl, p, nullptr);
    }
  }

void VPoolCache::notifyDestroy(const AbstractGraphicsApi::NoCopy* res) {
  std::lock_guard<std::mutex> guard(sync);

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

VkDescriptorPool VPoolCache::allocPool(const LayoutDesc &l) {
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

VkDescriptorSet VPoolCache::allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay) {
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

VPoolCache::Inst VPoolCache::allocBindless(const PushBlock& pb, const LayoutDesc& layout, const Bindings& binding) {
  auto lx = layout;
  for(uint32_t mask = lx.runtime; mask!=0;) {
    const int i = std::countr_zero(mask);
    mask ^= (1u << i);
    auto* a = reinterpret_cast<const VDescriptorArray*>(binding.data[i]);
    lx.count[i] = uint32_t(a->size());
    }

  Inst ret;
  ret.dLay = dev.setLayouts.findLayout(lx);
  ret.pLay = dev.psoLayouts.findLayout(pb, ret.dLay);

  std::lock_guard<std::mutex> guard(sync);
  for(auto& i:descriptors) {
    if(i.dLay!=ret.dLay || i.bindings!=binding)
      continue;
    ret.set = i.set;
    return ret;
    }

  auto& desc = descriptors.emplace_back();
  try {
    desc.dLay     = ret.dLay;
    desc.bindings = binding;
    desc.pool     = allocPool(lx);
    desc.set      = allocDescSet(desc.pool, ret.dLay);
    initDescriptorSet(desc.set, binding, lx);
    }
  catch(...) {
    if(desc.pool!=VK_NULL_HANDLE)
      vkDestroyDescriptorPool(dev.device.impl, desc.pool, nullptr);
    descriptors.pop_back();
    throw;
    }

  ret.set  = desc.set;
  return ret;
  }

void VPoolCache::initDescriptorSet(VkDescriptorSet dset, const Bindings &binding, const LayoutDesc& l) {
  VkCopyDescriptorSet    cpy[MaxBindings] = {};
  uint32_t               cntCpy = 0;

  VPushDescriptor::WriteInfo winfo[MaxBindings] = {};
  VkWriteDescriptorSet       wr   [MaxBindings] = {};
  uint32_t                   cntWr = 0;

  for(size_t i=0; i<MaxBindings; ++i) {
    if((l.active & (1u << i))==0)
      continue;

    VkCopyDescriptorSet& cx = cpy[cntCpy];
    if((binding.array & (1u << i))!=0) {
      // assert((l.runtime & (1u << i))!=0);
      auto arr = reinterpret_cast<const VDescriptorArray*>(binding.data[i]);
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
                           binding.data[i], binding.offset[i], binding.map[i], binding.smp[i]);

    VkWriteDescriptorSet& wx = wr[cntWr];
    wx.dstSet = dset;
    if(wx.descriptorCount>0)
      ++cntWr;
    }

  vkUpdateDescriptorSets(dev.device.impl, cntWr, wr, cntCpy, cpy);
  }

#endif
