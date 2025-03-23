#if defined(TEMPEST_BUILD_VULKAN)

#include "vpoolcache.h"
#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VPoolCache::VPoolCache(VDevice& dev)
  : dev(dev) {
  cache.reserve(MaxCache);
  }

VPoolCache::~VPoolCache() {
  for(auto& i:cache)
    vkDestroyDescriptorPool(dev.device.impl, i, nullptr);
  }

void VPoolCache::setupLimits(VulkanInstance& api) {
  if(api.hasDeviceFeatures2) {
    VkPhysicalDeviceAccelerationStructurePropertiesKHR rtas = {};
    rtas.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    properties.pNext = &rtas;

    auto vkGetPhysicalDeviceProperties2 = PFN_vkGetPhysicalDeviceProperties2(vkGetInstanceProcAddr(api.instance,"vkGetPhysicalDeviceProperties2KHR"));
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

#endif
