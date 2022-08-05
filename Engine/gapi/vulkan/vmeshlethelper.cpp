#if defined(TEMPEST_BUILD_VULKAN)

#include "vmeshlethelper.h"

#include "builtin_shader.h"

#include "vdevice.h"
#include "vbuffer.h"
#include "vshader.h"
#include "vpipelinelay.h"
#include "vpipeline.h"

using namespace Tempest::Detail;

VMeshletHelper::VMeshletHelper(VDevice& dev) : dev(dev) {
  const auto ind = MemUsage::StorageBuffer | MemUsage::Indirect | MemUsage::TransferDst;
  const auto ms  = MemUsage::StorageBuffer | MemUsage::TransferDst;
  const auto geo = MemUsage::StorageBuffer | MemUsage::VertexBuffer | MemUsage::IndexBuffer;

  indirect  = dev.allocator.alloc(nullptr, IndirectMemorySize, 1,1, ind, BufferHeap::Device);
  meshlets  = dev.allocator.alloc(nullptr, MeshletsMemorySize, 1,1, ms,  BufferHeap::Device);

  scratch   = dev.allocator.alloc(nullptr, PipelineMemorySize, 1,1, MemUsage::StorageBuffer, BufferHeap::Device);
  compacted = dev.allocator.alloc(nullptr, PipelineMemorySize, 1,1, geo, BufferHeap::Device);

  try {
    descLay  = initLayout(dev);
    descPool = initPool(dev);
    engSet   = initDescriptors(dev,descPool,descLay);
    initEngSet();
    initShaders(dev);
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

void VMeshletHelper::bindCS(VkCommandBuffer impl, VkPipelineLayout lay) {
  vkCmdBindDescriptorSets(impl,VK_PIPELINE_BIND_POINT_COMPUTE,
                          lay, 1,
                          1,&engSet,
                          0,nullptr);
  }

void VMeshletHelper::bindVS(VkCommandBuffer impl) {
  VkBuffer     buffers[1] = {compacted.impl};
  VkDeviceSize offsets[1] = {0};
  vkCmdBindVertexBuffers(impl, 0, 1, buffers, offsets);
  vkCmdBindIndexBuffer  (impl, compacted.impl, 0, VK_INDEX_TYPE_UINT32);
  }

void VMeshletHelper::initRP(VkCommandBuffer impl) {
  // VkDrawIndexedIndirectCommand cmd = {};
  // indirect.read(&cmd,0,sizeof(cmd));

  // IVec3 cmdSz = {};
  // meshlets.read(&cmdSz,0,sizeof(cmdSz));

  // drawcall-related parts should be set to zeros
  vkCmdFillBuffer(impl, indirect.impl, 0, VK_WHOLE_SIZE, 0);
  // {0, 1, 1, <undefined>}
  IVec3 meshletBufInit = {0,1,1};
  vkCmdUpdateBuffer(impl ,meshlets.impl, 0, sizeof(meshletBufInit), &meshletBufInit);

  VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  vkCmdPipelineBarrier(impl, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       0,
                       1, &barrier,
                       0, nullptr,
                       0, nullptr);
  }

void VMeshletHelper::sortPass(VkCommandBuffer impl, uint32_t meshCallsCount) {
  VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  vkCmdPipelineBarrier(impl, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       0,
                       1, &barrier,
                       0, nullptr,
                       0, nullptr);

  vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_COMPUTE,prefixSum.handler->impl);
  // vkCmdBindDescriptorSets(impl,VK_PIPELINE_BIND_POINT_COMPUTE, lay, 1, 1,&engSet, 0,nullptr);
  // vkCmdDispatch(impl, 1,1,1); // one threadgroup for prefix pass
  }

VkDescriptorSetLayout VMeshletHelper::initLayout(VDevice& device) {
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

VkDescriptorSet VMeshletHelper::initDescriptors(VDevice& device,
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

void VMeshletHelper::initEngSet() {
  VkDescriptorBufferInfo buf[3] = {};
  buf[0].buffer = indirect.impl;
  buf[0].offset = 0;
  buf[0].range  = VK_WHOLE_SIZE;

  buf[1].buffer = meshlets.impl;
  buf[1].offset = 0;
  buf[1].range  = VK_WHOLE_SIZE;

  buf[2].buffer = scratch.impl;
  buf[2].offset = 0;
  buf[2].range  = VK_WHOLE_SIZE;

  VkWriteDescriptorSet  write[3] = {};
  for(int i=0; i<3; ++i) {
    write[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write[i].dstSet          = engSet;
    write[i].dstBinding      = uint32_t(i);
    write[i].dstArrayElement = 0;
    write[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write[i].descriptorCount = 1;
    write[i].pBufferInfo     = &buf[i];
    }

  vkUpdateDescriptorSets(dev.device.impl, 3, write, 0, nullptr);
  }

void VMeshletHelper::initShaders(VDevice& device) {
  auto prefixSumCs = DSharedPtr<VShader*>(new VShader(device,mesh_prefix_pass_comp_sprv,sizeof(mesh_prefix_pass_comp_sprv)));

  prefixSumLay = DSharedPtr<VPipelineLay*> (new VPipelineLay (device,&prefixSumCs.handler->lay));
  prefixSum    = DSharedPtr<VCompPipeline*>(new VCompPipeline(device,*prefixSumLay.handler,*prefixSumCs.handler));


  }

#endif
