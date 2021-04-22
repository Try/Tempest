#if defined(TEMPEST_BUILD_VULKAN)

#include "vbuffer.h"

#include <Tempest/UniformsLayout>

#include "vdevice.h"
#include "vdescriptorarray.h"
#include "vtexture.h"
#include "vuniformslay.h"

using namespace Tempest;
using namespace Tempest::Detail;

VDescriptorArray::VDescriptorArray(VkDevice device, VUniformsLay& vlay)
  :device(device),lay(&vlay) {
  if(lay.handler->hasSSBO)
    ssbo.reset(new SSBO[vlay.lay.size()]);

  std::lock_guard<Detail::SpinLock> guard(vlay.sync);
  for(auto& i:vlay.pool){
    if(i.freeCount==0)
      continue;
    if(allocDescSet(i.impl,vlay.impl)) {
      pool=&i;
      pool->freeCount--;
      return;
      }
    }

  vlay.pool.emplace_back();
  auto& b = vlay.pool.back();
  b.impl  = allocPool(vlay,Detail::VUniformsLay::POOL_SIZE);
  if(!allocDescSet(b.impl,vlay.impl))
    throw std::bad_alloc();
  pool = &b;
  pool->freeCount--;
  }

VDescriptorArray::~VDescriptorArray() {
  if(desc==VK_NULL_HANDLE)
    return;
  Detail::VUniformsLay* layImpl = lay.handler;
  std::lock_guard<Detail::SpinLock> guard(layImpl->sync);

  vkFreeDescriptorSets(device,pool->impl,1,&desc);
  pool->freeCount++;
  }

VkDescriptorPool VDescriptorArray::allocPool(const VUniformsLay& lay, size_t size) {
  VkDescriptorPoolSize poolSize[3] = {};
  size_t               pSize=0;

  for(size_t i=0;i<lay.lay.size();++i){
    auto cls = lay.lay[i].cls;
    switch(cls) {
      case ShaderReflection::Ubo:     addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);         break;
      case ShaderReflection::Texture: addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); break;
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW:  addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);         break;
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW:   addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);          break;
      case ShaderReflection::Push:    break;
      }
    }

  for(auto& i:poolSize)
    i.descriptorCount*=uint32_t(size);

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = uint32_t(size);
  poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.poolSizeCount = uint32_t(pSize);
  poolInfo.pPoolSizes    = poolSize;

  VkDescriptorPool ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorPool(device,&poolInfo,nullptr,&ret));
  return ret;
  }

bool VDescriptorArray::allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay) {
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &lay;

  VkResult ret=vkAllocateDescriptorSets(device,&allocInfo,&desc);
  if(ret==VK_ERROR_FRAGMENTED_POOL)
    return false;

  vkAssert(ret);
  return true;
  }

void VDescriptorArray::set(size_t id, Tempest::AbstractGraphicsApi::Texture* t, const Sampler2d& smp) {
  VTexture* tex=reinterpret_cast<VTexture*>(t);

  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView   = tex->getView(device,smp.mapping,uint32_t(-1));

  tex->alloc->updateSampler(imageInfo.sampler,smp,tex->mipCnt);

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo      = &imageInfo;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray::setSsbo(size_t id, AbstractGraphicsApi::Texture* t, uint32_t mipLevel) {
  VTexture* tex=reinterpret_cast<VTexture*>(t);

  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageInfo.imageView   = tex->getView(device,ComponentMapping(),mipLevel);

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo      = &imageInfo;

  if(lay.handler->hasSSBO)
    ssbo[id].tex = t;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray::setUbo(size_t id, Tempest::AbstractGraphicsApi::Buffer *buf, size_t offset) {
  VBuffer* memory=reinterpret_cast<VBuffer*>(buf);
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = memory->impl;
  bufferInfo.offset = offset;
  bufferInfo.range  = lay.handler->lay[id].size;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo     = &bufferInfo;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray::setSsbo(size_t id, Tempest::AbstractGraphicsApi::Buffer *buf, size_t offset) {
  VBuffer* memory=reinterpret_cast<VBuffer*>(buf);
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = memory->impl;
  bufferInfo.offset = offset;
  bufferInfo.range  = lay.handler->lay[id].size;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo     = &bufferInfo;

  if(lay.handler->hasSSBO)
    ssbo[id].buf = buf;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray::ssboBarriers(ResourceState& res) {
  for(size_t i=0; i<lay.handler->lay.size(); ++i) {
    switch(lay.handler->lay[i].cls) {
      case ShaderReflection::Ubo:
      case ShaderReflection::Texture:
      case ShaderReflection::Push:
        break;
      case ShaderReflection::SsboR:
        res.setLayout(*ssbo[i].buf,BufferLayout::ComputeRead);
        break;
      case ShaderReflection::SsboRW:
        res.setLayout(*ssbo[i].buf,BufferLayout::ComputeReadWrite);
        break;
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW:
        break;
      }
    }
  }

void VDescriptorArray::addPoolSize(VkDescriptorPoolSize *p, size_t &sz, VkDescriptorType elt) {
  for(size_t i=0;i<sz;++i){
    if(p[i].type==elt) {
      p[i].descriptorCount++;
      return;
      }
    }
  p[sz].type=elt;
  p[sz].descriptorCount=1;
  sz++;
  }

#endif
