#include "vbuffer.h"

#include "vdevice.h"
#include "vdescriptorarray.h"

using namespace Tempest::Detail;

VDescriptorArray::~VDescriptorArray() {
  // It is invalid to call vkFreeDescriptorSets() with a pool
  // created without setting VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
  // vkFreeDescriptorSets(a->device,a->impl,a->count,a->desc);
  vkDestroyDescriptorPool(device,impl,nullptr);
  }

VDescriptorArray *VDescriptorArray::alloc(VkDevice device, VkDescriptorSetLayout lay, size_t size) {
  std::vector<VkDescriptorSetLayout> layouts(size,lay); //FIXME

  VDescriptorArray* a=reinterpret_cast<VDescriptorArray*>(std::malloc(sizeof(VDescriptorArray)+(size-1)*sizeof(VkDescriptorSet)));
  if(a==nullptr)
    throw std::bad_alloc();
  new(a) VDescriptorArray();

  VkDescriptorPoolSize poolSize = {};
  poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSize.descriptorCount = uint32_t(size);

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = uint32_t(size);
  poolInfo.poolSizeCount = 1;
  //poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.pPoolSizes    = &poolSize;

  vkAssert(vkCreateDescriptorPool(device,&poolInfo,nullptr,&a->impl));

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = a->impl;
  allocInfo.descriptorSetCount = size;
  allocInfo.pSetLayouts        = layouts.data();

  VkResult ret=vkAllocateDescriptorSets(device,&allocInfo,&a->desc[0]);
  if(ret!=VK_SUCCESS) {
    vkDestroyDescriptorPool(device,a->impl,nullptr);
    vkAssert(ret);
    }
  a->count  = size;
  a->device = device;
  return a;
  }

void VDescriptorArray::free(VDescriptorArray *a) {
  if(a==nullptr)
    return;
  a->~VDescriptorArray();
  std::free(a);
  }

void VDescriptorArray::bind(AbstractGraphicsApi::Buffer *mem, size_t size) {
  auto* memory=reinterpret_cast<VBuffer*>(mem);
  for(size_t i=0; i<count; i++) {
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = memory->impl;
    bufferInfo.offset = i*size;
    bufferInfo.range  = size;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = desc[i];
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo     = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
  }
