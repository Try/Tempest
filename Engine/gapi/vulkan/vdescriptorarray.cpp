#include "vbuffer.h"

#include <Tempest/UniformsLayout>

#include "vdevice.h"
#include "vdescriptorarray.h"
#include "vtexture.h"
#include "vuniformslay.h"

using namespace Tempest;
using namespace Tempest::Detail;

VDescriptorArray::VDescriptorArray(VkDevice device, const UniformsLayout& lay,
                                   std::shared_ptr<AbstractGraphicsApi::UniformsLay>& layP)
  :device(device),lay(layP) {
  Detail::VUniformsLay* layImpl = reinterpret_cast<Detail::VUniformsLay*>(this->lay.get());

  VkDescriptorPoolSize poolSize[3] = {{}};
  size_t pSize=0;

  for(size_t i=0;i<lay.size();++i){
    auto cls = lay[i].cls;
    switch(cls) {
      case UniformsLayout::Ubo:     addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);         break;
      case UniformsLayout::UboDyn:  addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC); break;
      case UniformsLayout::Texture: addPoolSize(poolSize,pSize,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); break;
      }
    }
  if(pSize==0)
    return;

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = 1;
  //poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.poolSizeCount = pSize;
  poolInfo.pPoolSizes    = poolSize;

  vkAssert(vkCreateDescriptorPool(device,&poolInfo,nullptr,&impl));

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = impl;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &layImpl->impl;

  VkResult ret=vkAllocateDescriptorSets(device,&allocInfo,desc);
  if(ret!=VK_SUCCESS) {
    vkDestroyDescriptorPool(device,impl,nullptr);
    vkAssert(ret);
    }
  }

VDescriptorArray::~VDescriptorArray() {
  // It is invalid to call vkFreeDescriptorSets() with a pool
  // created without setting VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
  // vkFreeDescriptorSets(a->device,a->impl,a->count,a->desc);
  vkDestroyDescriptorPool(device,impl,nullptr);
  }

void VDescriptorArray::set(size_t id,Tempest::AbstractGraphicsApi::Texture* t) {
  VTexture* tex=reinterpret_cast<VTexture*>(t);

  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView   = tex->view;
  imageInfo.sampler     = tex->sampler;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc[0];
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo      = &imageInfo;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray::set(size_t id, Tempest::AbstractGraphicsApi::Buffer *buf, size_t offset, size_t size) {
  VBuffer* memory=reinterpret_cast<VBuffer*>(buf);
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = memory->impl;
  bufferInfo.offset = offset;
  bufferInfo.range  = size;

  Detail::VUniformsLay* layImpl = reinterpret_cast<Detail::VUniformsLay*>(lay.get());

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc[0];
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = layImpl->hint[id];
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo     = &bufferInfo;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
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
