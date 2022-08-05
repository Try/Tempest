#if defined(TEMPEST_BUILD_VULKAN)

#include "vmeshlethelper.h"

#include "vdevice.h"
#include "vbuffer.h"

using namespace Tempest::Detail;

VMeshletHelper::VMeshletHelper(VDevice& dev) : dev(dev) {
  meshlets  = dev.allocator.alloc(nullptr, MeshletsMemorySize, 1,1, MemUsage::StorageBuffer, BufferHeap::Device);

  scratch   = dev.allocator.alloc(nullptr, PipelineMemorySize, 1,1, MemUsage::StorageBuffer, BufferHeap::Device);
  compacted = dev.allocator.alloc(nullptr, PipelineMemorySize, 1,1, MemUsage::StorageBuffer |
                                                                    MemUsage::VertexBuffer |
                                                                    MemUsage::IndexBuffer, BufferHeap::Device);

  try {
    descLay  = initLayout(dev);
    descPool = initPool(dev);
    engSet   = initDescriptors(dev,descPool,descLay);
    }
  catch(...) {
    cleanup();
    }
  }

VMeshletHelper::~VMeshletHelper() {
  cleanup();
  }

void VMeshletHelper::cleanup() {
  if(descLay!=VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(dev.device.impl, descLay, nullptr);
  if(engSet!=VK_NULL_HANDLE)
    vkFreeDescriptorSets(dev.device.impl, descPool, 1, &engSet);
  if(descPool!=VK_NULL_HANDLE)
    vkDestroyDescriptorPool(dev.device.impl, descPool, nullptr);
  }

VkDescriptorSetLayout Tempest::Detail::VMeshletHelper::initLayout(VDevice& device) {
  VkDescriptorSetLayoutBinding bind[3] = {};

  for(int i=0; i<3; ++i) {
    bind[i].binding         = i;
    bind[i].descriptorCount = 1;
    bind[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bind[i].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    }

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext        = nullptr;
  info.flags        = 0;
  info.bindingCount = 3;
  info.pBindings    = bind;

  VkDescriptorSetLayout ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorSetLayout(dev.device.impl,&info,nullptr,&ret));
  return ret;
  }

VkDescriptorPool VMeshletHelper::initPool(VDevice& device) {
  VkDescriptorPoolSize poolSize[1] = {};
  poolSize[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSize[0].descriptorCount = 3;

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = 1;
  poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.poolSizeCount = uint32_t(1);
  poolInfo.pPoolSizes    = poolSize;

  VkDevice dev = device.device.impl;
  VkDescriptorPool ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorPool(dev,&poolInfo,nullptr,&ret));
  return ret;
  }

VkDescriptorSet Tempest::Detail::VMeshletHelper::initDescriptors(VDevice& device,
                                                                 VkDescriptorPool pool,
                                                                 VkDescriptorSetLayout lay) {
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &lay;

  VkDevice        dev  = device.device.impl;
  VkDescriptorSet desc = VK_NULL_HANDLE;
  vkAssert(vkAllocateDescriptorSets(dev,&allocInfo,&desc));
  return desc;
  }

#endif
