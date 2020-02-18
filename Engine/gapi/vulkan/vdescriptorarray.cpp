#include "vbuffer.h"

#include <Tempest/UniformsLayout>

#include "vdevice.h"
#include "vdescriptorarray.h"
#include "vtexture.h"
#include "vuniformslay.h"

using namespace Tempest;
using namespace Tempest::Detail;

VDescriptorArray::VDescriptorArray(VkDevice device, const UniformsLayout& lay, VUniformsLay& vlay)
  :device(device),lay(&vlay) {
  if(vlay.hint.size()==0)
    return;

  if(vlay.offsetsCnt<=sizeof(multV)/sizeof(multV[0])) {
    multiplier = multV;
    } else {
    multiplier = new size_t[vlay.offsetsCnt];
    }

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
  b.impl  = allocPool(lay,Detail::VUniformsLay::POOL_SIZE);
  if(!allocDescSet(b.impl,vlay.impl))
    throw std::bad_alloc();
  pool = &b;
  pool->freeCount--;
  }

VDescriptorArray::~VDescriptorArray() {
  if(desc==VK_NULL_HANDLE)
    return;
  if(multiplier!=multV)
    delete multiplier;
  Detail::VUniformsLay* layImpl = lay.handler;
  std::lock_guard<Detail::SpinLock> guard(layImpl->sync);

  vkFreeDescriptorSets(device,pool->impl,1,&desc);
  pool->freeCount++;
  }

VkDescriptorPool VDescriptorArray::allocPool(const UniformsLayout& lay, size_t size) {
  VkDescriptorPoolSize poolSize[3] = {};
  size_t               pSize=0;

  for(size_t i=0;i<lay.size();++i){
    auto cls = lay[i].cls;
    switch(cls) {
      case UniformsLayout::Ubo:     addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);         break;
      case UniformsLayout::UboDyn:  addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC); break;
      case UniformsLayout::Texture: addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); break;
      }
    }

  for(auto& i:poolSize)
    i.descriptorCount*=size;

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = size;
  poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.poolSizeCount = pSize;
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

void VDescriptorArray::set(size_t id,Tempest::AbstractGraphicsApi::Texture* t) {
  VTexture* tex=reinterpret_cast<VTexture*>(t);

  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView   = tex->view;
  imageInfo.sampler     = tex->sampler;

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

void VDescriptorArray::set(size_t id, Tempest::AbstractGraphicsApi::Buffer *buf, size_t offset, size_t size, size_t align) {
  VBuffer* memory=reinterpret_cast<VBuffer*>(buf);
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = memory->impl;
  bufferInfo.offset = offset;
  bufferInfo.range  = size;

  Detail::VUniformsLay* layImpl = lay.handler;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = layImpl->hint[id];
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo     = &bufferInfo;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

  size_t alignId=size_t(-1);
  for(size_t i=0;i<=id;++i) {
    if(layImpl->hint[i]==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
      ++alignId;
    }
  if(alignId!=size_t(-1))
    multiplier[alignId] = align;
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
