#if defined(TEMPEST_BUILD_VULKAN)
#include "vbuffer.h"

#include "vdevice.h"
#include "vdescriptorarray.h"
#include "vtexture.h"
#include "vaccelerationstructure.h"

using namespace Tempest;
using namespace Tempest::Detail;

static VkImageLayout toWriteLayout(VTexture& tex) {
  if(nativeIsDepthFormat(tex.format))
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  if(tex.isStorageImage)
    return VK_IMAGE_LAYOUT_GENERAL;
  return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }


VDescriptorArray::VDescriptorArray(VDevice &dev, AbstractGraphicsApi::Texture **tex, size_t cnt, uint32_t mipLevel, const Sampler &smp)
  : dev(dev), cnt(cnt) {
  // Roundup a little, to avoid spamming of layouts
  const uint32_t cntRound = dev.roundUpDescriptorCount(ShaderReflection::Texture, cnt);

  const auto lay = dev.bindlessArrayLayout(ShaderReflection::Texture, cntRound);
  alloc(lay, dev, ShaderReflection::Texture, cntRound);
  populate(dev, tex, cnt, mipLevel, &smp);
  }

VDescriptorArray::VDescriptorArray(VDevice &dev, AbstractGraphicsApi::Texture **tex, size_t cnt, uint32_t mipLevel)
  : dev(dev), cnt(cnt) {
  // Roundup a little, to avoid spamming of layouts
  const uint32_t cntRound = dev.roundUpDescriptorCount(ShaderReflection::Image, cnt);

  const auto lay = dev.bindlessArrayLayout(ShaderReflection::Image, cntRound);
  alloc(lay, dev, ShaderReflection::Image, cntRound);
  populate(dev, tex, cnt, mipLevel, nullptr);
  }

VDescriptorArray::VDescriptorArray(VDevice &dev, AbstractGraphicsApi::Buffer **buf, size_t cnt)
  : dev(dev), cnt(cnt) {
  // Roundup a little, to avoid spamming of layouts
  const uint32_t cntRound = dev.roundUpDescriptorCount(ShaderReflection::SsboRW, cnt);

  const auto lay = dev.bindlessArrayLayout(ShaderReflection::SsboRW, cntRound);
  alloc(lay, dev, ShaderReflection::SsboRW, cntRound);
  populate(dev, buf, cnt);
  }

VDescriptorArray::~VDescriptorArray() {
  dev.bindless.notifyDestroy(this);
  if(pool!=VK_NULL_HANDLE)
    vkDestroyDescriptorPool(dev.device.impl, pool, nullptr);
  }

size_t VDescriptorArray::size() const {
  return cnt;
  }

void VDescriptorArray::alloc(VkDescriptorSetLayout lay, VDevice &dev, ShaderReflection::Class cls, size_t cnt) {
  VkDescriptorPoolSize       poolSize = {};
  poolSize.type            = nativeFormat(cls);
  poolSize.descriptorCount = uint32_t(cnt);

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = 1;
  poolInfo.flags         = 0; // VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_EXT;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes    = &poolSize;
  poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  vkAssert(vkCreateDescriptorPool(dev.device.impl, &poolInfo, nullptr, &pool));

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  //allocInfo.pNext              = &vinfo;
  allocInfo.descriptorPool     = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &lay;
  vkAssert(vkAllocateDescriptorSets(dev.device.impl,&allocInfo,&dset));
  }

void VDescriptorArray::populate(VDevice &dev, AbstractGraphicsApi::Texture **t, size_t cnt, uint32_t mipLevel, const Sampler* smp) {
  const bool is3DImage = false; //TODO

  // 16 is non-bindless limit
  SmallArray<VkDescriptorImageInfo,16> imageInfo(cnt);
  for(size_t i=0; i<cnt; ++i) {
    if(t[i]==nullptr) {
      imageInfo[i] = VkDescriptorImageInfo{};
      continue;
      }
    VTexture& tex = *reinterpret_cast<VTexture*>(t[i]);
    imageInfo[i].imageLayout = toWriteLayout(tex);
    imageInfo[i].imageView   = tex.view(ComponentMapping(), mipLevel, is3DImage);
    imageInfo[i].sampler     = smp!=nullptr ? dev.allocator.updateSampler(*smp) : VK_NULL_HANDLE;
    // TODO: support mutable textures in bindless
    assert(tex.nonUniqId==0);
    }

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = dset;
  descriptorWrite.dstBinding      = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = smp!=nullptr ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptorWrite.descriptorCount = uint32_t(cnt);
  descriptorWrite.pImageInfo      = imageInfo.get();

  vkUpdateDescriptorSets(dev.device.impl, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray::populate(VDevice &dev, AbstractGraphicsApi::Buffer **b, size_t cnt) {
  // 16 is non-bindless limit
  SmallArray<VkDescriptorBufferInfo,16> bufInfo(cnt);
  for(size_t i=0; i<cnt; ++i) {
    VBuffer* buf = reinterpret_cast<VBuffer*>(b[i]);
    bufInfo[i].buffer = buf!=nullptr ? buf->impl : VK_NULL_HANDLE;
    bufInfo[i].offset = 0;
    bufInfo[i].range  = VK_WHOLE_SIZE;
    if(!dev.props.hasRobustness2 && buf==nullptr) {
      bufInfo[i].buffer = dev.dummySsbo().impl;
      bufInfo[i].offset = 0;
      bufInfo[i].range  = 0;
      }
    // assert(buf->nonUniqId==0);
    }

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = dset;
  descriptorWrite.dstBinding      = uint32_t(0);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorWrite.descriptorCount = uint32_t(cnt);
  descriptorWrite.pBufferInfo     = bufInfo.get();

  vkUpdateDescriptorSets(dev.device.impl, 1, &descriptorWrite, 0, nullptr);
  }

#endif
