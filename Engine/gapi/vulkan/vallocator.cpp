  #include "vallocator.h"

#include "vdevice.h"
#include "vbuffer.h"

using namespace Tempest::Detail;

VAllocator::VAllocator() {
  }

void VAllocator::setDevice(VDevice &dev) {
  device          = dev.device;
  provider.device = &dev;
  }

VAllocator::Provider::DeviceMemory VAllocator::Provider::alloc(size_t size,uint32_t typeBits) {
  VkDeviceMemory   memory=VK_NULL_HANDLE;

  VkMemoryAllocateInfo vk_memoryAllocateInfo;
  vk_memoryAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  vk_memoryAllocateInfo.pNext           = nullptr;
  vk_memoryAllocateInfo.allocationSize  = size;
  vk_memoryAllocateInfo.memoryTypeIndex = device->memoryTypeIndex(typeBits);

  if(vkAllocateMemory(device->device,&vk_memoryAllocateInfo,nullptr,&memory)!=VkResult::VK_SUCCESS)
    throw std::runtime_error("failed to allocate device memory");
  return memory;
  }

void VAllocator::Provider::free(VAllocator::Provider::DeviceMemory m) {
  vkFreeMemory(device->device,m,nullptr);
  }

VBuffer VAllocator::alloc(const void *mem, size_t size, AbstractGraphicsApi::MemUsage usage) {
  VBuffer ret;
  ret.alloc = this;

  VkBufferCreateInfo createInfo={};
  createInfo.sType                 = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  createInfo.pNext                 = nullptr;
  createInfo.flags                 = 0;
  createInfo.size                  = size;
  createInfo.sharingMode           = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
  createInfo.queueFamilyIndexCount = 0;
  createInfo.pQueueFamilyIndices   = nullptr;

  if(uint8_t(usage) & uint8_t(AbstractGraphicsApi::MemUsage::UniformBit))
    createInfo.usage |= VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  if(uint8_t(usage) & uint8_t(AbstractGraphicsApi::MemUsage::VertexBuffer))
    createInfo.usage |= VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  if(vkCreateBuffer(device,&createInfo,nullptr,&ret.impl)!=VkResult::VK_SUCCESS)
    throw std::runtime_error("failed to create buffer");

  VkMemoryRequirements memRq;
  vkGetBufferMemoryRequirements(device,ret.impl,&memRq);

  ret.page=allocator.alloc(size_t(memRq.size),size_t(memRq.alignment),memRq.memoryTypeBits);
  if(!ret.page.page)
    throw std::runtime_error("failed to allocate memory");
  commit(ret.page.page->memory,ret.impl,mem,ret.page.offset,size);
  return ret;
  }

void VAllocator::free(VBuffer &buf) {
  vkDeviceWaitIdle(device);
  vkDestroyBuffer (device,buf.impl,nullptr);

  allocator.free(buf.page);
  }

void VAllocator::commit(VkDeviceMemory dev,VkBuffer dest,const void* mem,size_t offset,size_t size) {
  void* data=nullptr;
  if(vkMapMemory(device,dev,offset,size,0,&data)!=VkResult::VK_SUCCESS)
    throw std::runtime_error("failed to allocate device memory");
  memcpy(data,mem,size);
  vkUnmapMemory(device,dev);

  if(vkBindBufferMemory(device,dest,dev,offset)!=VkResult::VK_SUCCESS)
    throw std::runtime_error("failed to allocate device memory");
  }
