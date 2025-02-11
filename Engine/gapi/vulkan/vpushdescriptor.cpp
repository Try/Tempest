#if defined(TEMPEST_BUILD_VULKAN)
#include "vpushdescriptor.h"

#include "gapi/vulkan/vaccelerationstructure.h"
#include "gapi/vulkan/vtexture.h"
#include "gapi/vulkan/vpipelinelay.h"
#include "gapi/vulkan/vpipeline.h"
#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

static VkImageLayout toWriteLayout(VTexture& tex) {
  if(nativeIsDepthFormat(tex.format))
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  if(tex.isStorageImage)
    return VK_IMAGE_LAYOUT_GENERAL;
  return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

VPushDescriptor::Pool::Pool(VDevice &dev) {
  VkDescriptorPoolSize poolSize[int(ShaderReflection::Class::Count)] = {};
  size_t               pSize   = 0;

  VkPhysicalDeviceProperties prop={};
  vkGetPhysicalDeviceProperties(dev.physicalDevice, &prop);

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
        sz.descriptorCount = std::min(prop.limits.maxDescriptorSetUniformBuffers, maxResources);
        break;
        }
      case ShaderReflection::Texture: {
        sz.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sz.descriptorCount = std::min(prop.limits.maxDescriptorSetSampledImages, maxResources);
        sz.descriptorCount = std::min(prop.limits.maxDescriptorSetSamplers, sz.descriptorCount);
        sz.descriptorCount = std::min(maxSamplers, sz.descriptorCount);
        break;
        }
      case ShaderReflection::Image: {
        sz.type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sz.descriptorCount = std::min(prop.limits.maxDescriptorSetSampledImages, maxResources);
        break;
        }
      case ShaderReflection::Sampler: {
        sz.type            = VK_DESCRIPTOR_TYPE_SAMPLER;
        sz.descriptorCount = std::min(prop.limits.maxDescriptorSetSamplers, maxSamplers);
        break;
        }
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW: {
        sz.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sz.descriptorCount = std::min(prop.limits.maxDescriptorSetStorageBuffers, maxResources);
        break;
        }
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW: {
        sz.type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        sz.descriptorCount = std::min(prop.limits.maxDescriptorSetStorageImages, maxResources);
        break;
        }
      case ShaderReflection::Tlas: {
        if(dev.props.raytracing.rayQuery) {
          sz.type            = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
          sz.descriptorCount = 16; // conservative limit
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

  vkAssert(vkCreateDescriptorPool(dev.device.impl, &poolInfo, nullptr, &impl));
  }


VPushDescriptor::VPushDescriptor(VDevice &dev)
  :dev(dev) {
  }

VPushDescriptor::~VPushDescriptor() {
  reset();
  }

void VPushDescriptor::reset() {
  pool.reserve(pool.size());
  for(auto& i:pool) {
    //TODO: reset and cache it globally
    vkDestroyDescriptorPool(dev.device.impl, i.impl, nullptr);
    }
  pool.clear();
  }

VkDescriptorSet VPushDescriptor::allocSet(const VkDescriptorSetLayout dLayout) {
  if(pool.empty())
    pool.emplace_back(Pool(dev));

  for(int i=0; i<2; ++i) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool.back().impl;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &dLayout;

    VkDescriptorSet desc = VK_NULL_HANDLE;
    VkResult        ret  = vkAllocateDescriptorSets(dev.device.impl, &allocInfo, &desc);
    if(ret==VK_ERROR_FRAGMENTED_POOL || ret==VK_ERROR_OUT_OF_POOL_MEMORY) {
      pool.emplace_back(Pool(dev));
      continue;
      }
    vkAssert(ret);
    return desc;
    }
  vkAssert(VK_ERROR_OUT_OF_HOST_MEMORY);
  return VK_NULL_HANDLE;
  }

VkDescriptorSet VPushDescriptor::allocSet(const LayoutDesc& lx) {
  auto lt = dev.setLayouts.findLayout(lx);
  return allocSet(lt);
  }

VkDescriptorSet VPushDescriptor::push(const PushBlock& pb, const LayoutDesc& layout, const Bindings& binding) {
  auto set = allocSet(layout);

  WriteInfo              winfo[MaxBindings] = {};
  VkWriteDescriptorSet   wr   [MaxBindings] = {};
  uint32_t               cntWr = 0;

  for(size_t i=0; i<MaxBindings; ++i) {
    auto  cls = layout.bindings[i];
    auto& wx  = wr[cntWr];
    VPushDescriptor::write(dev, wx, winfo[cntWr], uint32_t(i), cls,
                           binding.data[i], binding.offset[i], binding.smp[i]);
    wx.dstSet = set;
    if(wx.descriptorCount>0)
      ++cntWr;
    }

  vkUpdateDescriptorSets(dev.device.impl, cntWr, wr, 0, nullptr);
  return set;
  }

void VPushDescriptor::write(VDevice& dev, VkWriteDescriptorSet& wx, WriteInfo& infoW, uint32_t dstBinding,
                            ShaderReflection::Class cls, AbstractGraphicsApi::NoCopy* data, uint32_t offset, const Sampler& smp) {
  switch(cls) {
    case ShaderReflection::Ubo:
    case ShaderReflection::SsboR:
    case ShaderReflection::SsboRW: {
      auto* buf = reinterpret_cast<VBuffer*>(data);

      VkDescriptorBufferInfo& info = infoW.buffer;
      info.buffer = buf!=nullptr ? buf->impl : VK_NULL_HANDLE;
      info.offset = offset;
      //TODO: head only ssbo
      info.range  = VK_WHOLE_SIZE; //(buf!=nullptr && slot.varByteSize==0) ? slot.byteSize : VK_WHOLE_SIZE;

      if(!dev.props.hasRobustness2 && buf==nullptr) {
        //NOTE1: use of null-handle is not allowed, unless VK_EXT_robustness2
        //NOTE2: sizeof 1 is rouned up in shader; and sizeof 0 is illegal but harmless(hepefully)
        info.buffer = dev.dummySsbo().impl;
        info.offset = 0;
        info.range  = 0;
        }

      wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wx.dstSet          = VK_NULL_HANDLE;
      wx.dstBinding      = dstBinding;
      wx.dstArrayElement = 0;
      wx.descriptorType  = nativeFormat(cls);
      wx.descriptorCount = 1;
      wx.pBufferInfo     = &info;
      break;
      }
    case ShaderReflection::Texture:
    case ShaderReflection::Image:
    case ShaderReflection::ImgR:
    case ShaderReflection::ImgRW: {
      auto*    tex       = reinterpret_cast<VTexture*>(data);
      uint32_t mipLevel  = offset;
      bool     is3DImage = tex->is3D; // TODO: cast 3d to 2d, based on dest descriptor

      VkDescriptorImageInfo& info = infoW.image;
      if(cls==ShaderReflection::Texture) {
        auto sx = smp;
        if(!tex->isFilterable) {
          sx.setFiltration(Filter::Nearest);
          sx.anisotropic = false;
          }
        info.sampler   = dev.allocator.updateSampler(sx);
        info.imageView = tex->view(sx.mapping, mipLevel, is3DImage);
        } else {
        info.imageView = tex->view(ComponentMapping(), mipLevel, is3DImage);
        }
      info.imageLayout = toWriteLayout(*tex);

      wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wx.dstSet          = VK_NULL_HANDLE;
      wx.dstBinding      = dstBinding;
      wx.dstArrayElement = 0;
      wx.descriptorType  = nativeFormat(cls);
      wx.descriptorCount = 1;
      wx.pImageInfo      = &info;
      break;
      }
    case ShaderReflection::Sampler: {
      VkDescriptorImageInfo& info = infoW.image;
      info.sampler = dev.allocator.updateSampler(smp);

      wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wx.dstSet          = VK_NULL_HANDLE;
      wx.dstBinding      = dstBinding;
      wx.dstArrayElement = 0;
      wx.descriptorType  = nativeFormat(cls);
      wx.descriptorCount = 1;
      wx.pImageInfo      = &info;
      break;
      }
    case ShaderReflection::Tlas: {
      auto* tlas = reinterpret_cast<VAccelerationStructure*>(data);

      auto& info = infoW.tlas;
      info.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
      info.accelerationStructureCount = 1;
      info.pAccelerationStructures    = &tlas->impl;

      wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wx.pNext           = &info;
      wx.dstSet          = VK_NULL_HANDLE;
      wx.dstBinding      = dstBinding;
      wx.dstArrayElement = 0;
      wx.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
      wx.descriptorCount = 1;
      break;
      }
    case ShaderReflection::Push:
    case ShaderReflection::Count:
      break;
    }
  }

#endif

