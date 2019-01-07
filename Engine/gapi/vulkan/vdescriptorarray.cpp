#include "vbuffer.h"

#include "vdevice.h"
#include "vdescriptorarray.h"
#include "vtexture.h"

using namespace Tempest::Detail;

VDescriptorArray::~VDescriptorArray() {
  // It is invalid to call vkFreeDescriptorSets() with a pool
  // created without setting VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
  // vkFreeDescriptorSets(a->device,a->impl,a->count,a->desc);
  vkDestroyDescriptorPool(device,impl,nullptr);
  }

VDescriptorArray *VDescriptorArray::alloc(VkDevice device, VkDescriptorSetLayout lay) {
  VDescriptorArray* a=reinterpret_cast<VDescriptorArray*>(std::malloc(sizeof(VDescriptorArray)));
  if(a==nullptr)
    throw std::bad_alloc();
  new(a) VDescriptorArray();

  std::array<VkDescriptorPoolSize,2> poolSize = {{}};
  poolSize[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSize[0].descriptorCount = 1;

  poolSize[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize[1].descriptorCount = 1;

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = 1;
  //poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.poolSizeCount = poolSize.size();
  poolInfo.pPoolSizes    = poolSize.data();

  vkAssert(vkCreateDescriptorPool(device,&poolInfo,nullptr,&a->impl));

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = a->impl;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &lay;

  VkResult ret=vkAllocateDescriptorSets(device,&allocInfo,&a->desc[0]);
  if(ret!=VK_SUCCESS) {
    vkDestroyDescriptorPool(device,a->impl,nullptr);
    vkAssert(ret);
    }
  a->count  = 1;
  a->device = device;
  return a;
  }

void VDescriptorArray::free(VDescriptorArray *a) {
  if(a==nullptr)
    return;
  a->~VDescriptorArray();
  std::free(a);
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

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc[0];
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo     = &bufferInfo;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
  }
