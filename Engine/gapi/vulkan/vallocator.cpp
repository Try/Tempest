#if defined(TEMPEST_BUILD_VULKAN)

#include "vallocator.h"

#include "vdevice.h"
#include "vbuffer.h"
#include "vtexture.h"

#include "exceptions/exception.h"

#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <thread>

using namespace Tempest;
using namespace Tempest::Detail;

static NonUniqResId nextId(){
  static std::atomic_uint32_t i = {};
  uint32_t ix = i.fetch_add(1) % 32;
  uint32_t b = 1u << ix;
  return NonUniqResId(b);
  }

VAllocator::VAllocator() {
  }

VAllocator::~VAllocator() {
  }

void VAllocator::setDevice(VDevice &d) {
  dev             = d.device.impl;
  provider.device = &d;
  samplers.setDevice(d);
  }

VDevice* VAllocator::device() {
  return provider.device;
  }

VAllocator::Provider::~Provider() {
  if(lastFree!=VK_NULL_HANDLE)
    vkFreeMemory(device->device.impl,lastFree,nullptr);
  }

VAllocator::Provider::DeviceMemory VAllocator::Provider::alloc(size_t size, uint32_t typeId) {
  if(lastFree!=VK_NULL_HANDLE){
    if(lastType==typeId && lastSize==size){
      VkDeviceMemory memory=lastFree;
      lastFree=VK_NULL_HANDLE;
      return memory;
      }
    vkFreeMemory(device->device.impl,lastFree,nullptr);
    lastFree=VK_NULL_HANDLE;
    }
  VkDeviceMemory memory=VK_NULL_HANDLE;

  VkMemoryAllocateInfo memoryAllocateInfo;
  memoryAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.pNext           = nullptr;
  memoryAllocateInfo.allocationSize  = size;
  memoryAllocateInfo.memoryTypeIndex = typeId;

  VkMemoryAllocateFlagsInfo flagsInfo = {};
  if(device->props.hasDeviceAddress) {
    // condition?
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    memoryAllocateInfo.pNext = &flagsInfo;
    }

  auto code = vkAllocateMemory(device->device.impl,&memoryAllocateInfo,nullptr,&memory);
  if(code!=VK_SUCCESS)
    return VK_NULL_HANDLE;
  return memory;
  }

void VAllocator::Provider::free(VAllocator::Provider::DeviceMemory m, size_t size, uint32_t typeId) {
  if(lastFree!=VK_NULL_HANDLE)
    vkFreeMemory(device->device.impl,lastFree,nullptr);

  lastFree = m;
  lastSize = size;
  lastType = typeId;
  }

static size_t GCD(size_t n1, size_t n2) {
  if(n1==1 || n2==1)
    return 1;

  while(n1!=n2) {
    if(n1<n2)
      n2 = n2 - n1; else
      n1 = n1 - n2;
    }
  return n1;
  }

static size_t LCM(size_t n1, size_t n2)  {
  return n1*n2 / GCD(n1, n2);
  }

VBuffer VAllocator::alloc(const void *mem, size_t size, MemUsage usage, BufferHeap bufHeap) {
  VBuffer ret;
  ret.alloc = this;
  if( MemUsage::StorageBuffer==(usage&MemUsage::StorageBuffer) ||
      MemUsage::TransferDst  ==(usage&MemUsage::TransferDst) ||
      MemUsage::AsStorage    ==(usage&MemUsage::AsStorage)) {
    ret.nonUniqId = nextId();
    }

  VkBufferCreateInfo createInfo={};
  createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  createInfo.pNext                 = nullptr;
  createInfo.flags                 = 0;
  createInfo.size                  = size;
  createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
  createInfo.queueFamilyIndexCount = 0;
  createInfo.pQueueFamilyIndices   = nullptr;

  if(MemUsage::TransferSrc==(usage & MemUsage::TransferSrc))
    createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  if(MemUsage::TransferDst==(usage & MemUsage::TransferDst))
    createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if(MemUsage::VertexBuffer==(usage & MemUsage::VertexBuffer))
    createInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if(MemUsage::IndexBuffer==(usage & MemUsage::IndexBuffer))
    createInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if(MemUsage::UniformBuffer==(usage & MemUsage::UniformBuffer))
    createInfo.usage |= (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  if(MemUsage::StorageBuffer==(usage & MemUsage::StorageBuffer))
    createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  if(provider.device->props.raytracing.rayQuery) {
    if(MemUsage::StorageBuffer==(usage & MemUsage::StorageBuffer)) {
      createInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
      }
    if(MemUsage::ScratchBuffer==(usage & MemUsage::ScratchBuffer)) {
      createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      }
    if(MemUsage::AsStorage==(usage & MemUsage::AsStorage))
      createInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    if(MemUsage::VertexBuffer==(usage & MemUsage::VertexBuffer) ||
       MemUsage::IndexBuffer ==(usage & MemUsage::IndexBuffer))
      createInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }

  if(provider.device->props.hasDeviceAddress) {
    createInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

  if(MemUsage::Indirect==(usage & MemUsage::Indirect))
    createInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

  vkAssert(vkCreateBuffer(dev,&createInfo,nullptr,&ret.impl));

  MemRequirements memRq={};
  getMemoryRequirements(memRq,ret.impl);
  if(MemUsage::ScratchBuffer==(usage & MemUsage::ScratchBuffer)) {
    memRq.alignment = std::max(memRq.alignment,
                               provider.device->props.accelerationStructureScratchOffsetAlignment);
    }

  uint32_t props[2] = {};
  uint8_t  propsCnt = 1;
  if(bufHeap==BufferHeap::Upload && usage==MemUsage::UniformBuffer) {
    propsCnt = 2;
    props[0] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    props[1] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
  else if(bufHeap==BufferHeap::Upload && (usage & MemUsage::StorageBuffer)==MemUsage::StorageBuffer) {
    propsCnt = 2;
    props[0] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    props[1] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
  else if(bufHeap==BufferHeap::Upload)
    props[0] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  else if(bufHeap==BufferHeap::Device)
    props[0] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  else if(bufHeap==BufferHeap::Readback)
    props[0] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

  for(uint8_t i=0; i<propsCnt; ++i) {
    auto p = VkMemoryPropertyFlagBits(props[i]);
    VDevice::MemIndex memId = provider.device->memoryTypeIndex(memRq.memoryTypeBits,p,VK_IMAGE_TILING_LINEAR);
    if(memId.typeId==uint32_t(-1))
      continue;

    ret.page = allocMemory(memRq,memId.heapId,memId.typeId,memId.hostVisible);
    if(!ret.page.page)
      continue;

    if(!commit(ret.page.page->memory,ret.page.page->mmapSync,ret.impl,ret.page.offset,mem,size)) {
      throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
      }
    return ret;
    }

  throw std::system_error(Tempest::GraphicsErrc::OutOfVideoMemory);
  }

VTexture VAllocator::alloc(const Pixmap& pm, uint32_t mip, VkFormat format) {
  VTexture ret;
  ret.alloc     = this;

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType     = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width  = pm.w();
  imageInfo.extent.height = pm.h();
  imageInfo.extent.depth  = 1;
  imageInfo.mipLevels     = mip;
  imageInfo.arrayLayers   = 1;
  imageInfo.format        = format;
  imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  vkAssert(vkCreateImage(dev, &imageInfo, nullptr, &ret.impl));

  MemRequirements memRq={};
  getImgMemoryRequirements(memRq,ret.impl);

  VDevice::MemIndex memId = provider.device->memoryTypeIndex(memRq.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,VK_IMAGE_TILING_OPTIMAL);
  ret.page = allocMemory(memRq,memId.heapId,memId.typeId,false);

  if(!ret.page.page) {
    ret.alloc = nullptr;
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }
  if(!commit(ret.page.page->memory,ret.page.page->mmapSync,ret.impl,ret.page.offset)) {
    ret.alloc = nullptr;
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }

  ret.format = imageInfo.format;
  ret.mipCnt = mip;
  ret.createViews(dev);
  return ret;
  }

VTexture VAllocator::alloc(const uint32_t w, const uint32_t h, const uint32_t d, const uint32_t mip, TextureFormat frm, bool imageStore) {
  VTexture ret;
  ret.alloc     = this;
  ret.nonUniqId = (imageStore) ?  nextId() : NonUniqResId::I_None;

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType     = d<=1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
  imageInfo.extent.width  = w;
  imageInfo.extent.height = h;
  imageInfo.extent.depth  = d;
  imageInfo.mipLevels     = mip;
  imageInfo.arrayLayers   = 1;
  imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage         = isDepthFormat(frm) ?
        (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT) :
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.format        = nativeFormat(frm);

  if(imageStore)
    imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

  vkAssert(vkCreateImage(dev, &imageInfo, nullptr, &ret.impl));

  MemRequirements memRq={};
  getImgMemoryRequirements(memRq,ret.impl);

  VDevice::MemIndex memId = provider.device->memoryTypeIndex(memRq.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,VK_IMAGE_TILING_OPTIMAL);
  ret.page = allocMemory(memRq,memId.heapId,memId.typeId,false);

  if(!ret.page.page) {
    ret.alloc = nullptr;
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }
  if(!commit(ret.page.page->memory,ret.page.page->mmapSync,ret.impl,ret.page.offset)) {
    ret.alloc = nullptr;
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }

  ret.format         = imageInfo.format;
  ret.mipCnt         = mip;
  ret.isStorageImage = imageStore;
  ret.is3D           = (imageInfo.imageType==VK_IMAGE_TYPE_3D);
  ret.isFilterable   = (provider.device->props.hasFilteredFormat(frm));
  ret.createViews(dev);
  return ret;
  }

void VAllocator::free(VAllocator::Allocation &page) {
  if(page.page!=nullptr)
    allocator.free(page);
  }

void VAllocator::free(VTexture &buf) {
  if(buf.imgView!=VK_NULL_HANDLE) {
    buf.destroyViews(dev);
    vkDestroyImage  (dev,buf.impl,nullptr);
    }
  else if(buf.impl!=VK_NULL_HANDLE) {
    vkDestroyImage  (dev,buf.impl,nullptr);
    }
  if(buf.page.page!=nullptr)
    allocator.free(buf.page);
  }

void VAllocator::getMemoryRequirements(MemRequirements& out,VkBuffer buf) {
  if(provider.device->props.hasMemRq2) {
    VkBufferMemoryRequirementsInfo2KHR bufInfo = {};
    bufInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR;
    bufInfo.buffer = buf;

    VkMemoryDedicatedRequirementsKHR memDedicatedRq = {};
    memDedicatedRq.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

    VkMemoryRequirements2KHR memReq2 = {};
    memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
    if(provider.device->props.hasDedicatedAlloc)
      memReq2.pNext = &memDedicatedRq;

    provider.device->vkGetBufferMemoryRequirements2(dev,&bufInfo,&memReq2);

    out.size           = size_t(memReq2.memoryRequirements.size);
    out.alignment      = size_t(memReq2.memoryRequirements.alignment);
    out.memoryTypeBits = memReq2.memoryRequirements.memoryTypeBits;
    out.dedicated =
        (memDedicatedRq.requiresDedicatedAllocation != VK_FALSE) ||
        (memDedicatedRq.prefersDedicatedAllocation  != VK_FALSE);
    out.dedicatedRq = (memDedicatedRq.requiresDedicatedAllocation != VK_FALSE);
    return;
    }

  VkMemoryRequirements memRq={};
  vkGetBufferMemoryRequirements(dev,buf,&memRq);
  out.size           = size_t(memRq.size);
  out.alignment      = size_t(memRq.alignment);
  out.memoryTypeBits = memRq.memoryTypeBits;
  }

void VAllocator::getImgMemoryRequirements(MemRequirements& out, VkImage img) {
  if(provider.device->props.hasMemRq2) {
    VkImageMemoryRequirementsInfo2 bufInfo = {};
    bufInfo.sType  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
    bufInfo.image  = img;

    VkMemoryDedicatedRequirementsKHR memDedicatedRq = {};
    memDedicatedRq.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

    VkMemoryRequirements2KHR memReq2 = {};
    memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
    if(provider.device->props.hasDedicatedAlloc)
      memReq2.pNext = &memDedicatedRq;

    provider.device->vkGetImageMemoryRequirements2(dev,&bufInfo,&memReq2);

    out.size           = size_t(memReq2.memoryRequirements.size);
    out.alignment      = size_t(memReq2.memoryRequirements.alignment);
    out.memoryTypeBits = memReq2.memoryRequirements.memoryTypeBits;
    out.dedicated =
        (memDedicatedRq.requiresDedicatedAllocation != VK_FALSE) ||
        (memDedicatedRq.prefersDedicatedAllocation  != VK_FALSE);
    return;
    }

  VkMemoryRequirements memRq={};
  vkGetImageMemoryRequirements(dev,img,&memRq);
  out.size           = size_t(memRq.size);
  out.alignment      = size_t(memRq.alignment);
  out.memoryTypeBits = memRq.memoryTypeBits;
  }

VAllocator::Allocation VAllocator::allocMemory(const VAllocator::MemRequirements& memRq,
                                               const uint32_t heapId, const uint32_t typeId, bool hostVisible) {
  const size_t align = LCM(memRq.alignment,provider.device->props.nonCoherentAtomSize);
  Allocation ret;
  if(memRq.dedicated) {
    ret = allocator.dedicatedAlloc(memRq.size,align,heapId,typeId,hostVisible);
    if(!ret.page && !memRq.dedicatedRq)
      ret = allocator.alloc(memRq.size,align,heapId,typeId,hostVisible);
    } else {
    ret = allocator.alloc(memRq.size,align,heapId,typeId,hostVisible);
    }
  return ret;
  }

void VAllocator::alignRange(VkMappedMemoryRange& rgn, size_t nonCoherentAtomSize, size_t& shift) {
  shift = rgn.offset%nonCoherentAtomSize;
  rgn.offset -= shift;
  rgn.size   += shift;

  if(rgn.size%nonCoherentAtomSize!=0)
    rgn.size += nonCoherentAtomSize-rgn.size%nonCoherentAtomSize;
  }

bool VAllocator::fill(VBuffer& dest, uint32_t mem, size_t offset, size_t size) {
  auto& page = dest.page;
  void* data = nullptr;

  VkMappedMemoryRange rgn={};
  rgn.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  rgn.memory = page.page->memory;
  rgn.offset = page.offset+offset;
  rgn.size   = size;

  size_t shift = 0;
  alignRange(rgn,provider.device->props.nonCoherentAtomSize,shift);

  std::lock_guard<std::mutex> g(page.page->mmapSync);
  if(vkMapMemory(dev,page.page->memory,rgn.offset,rgn.size,0,&data)!=VK_SUCCESS)
    return false;

  data = reinterpret_cast<uint8_t*>(data) + shift;
  std::fill_n(reinterpret_cast<uint32_t*>(data), size/sizeof(uint32_t), mem);

  vkFlushMappedMemoryRanges(dev,1,&rgn);
  vkUnmapMemory(dev,page.page->memory);
  return true;
  }

bool VAllocator::update(VBuffer &dest, const void *mem, size_t offset, size_t size) {
  auto& page = dest.page;
  void* data = nullptr;

  VkMappedMemoryRange rgn={};
  rgn.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  rgn.memory = page.page->memory;
  rgn.offset = page.offset+offset;
  rgn.size   = size;

  size_t shift = 0;
  alignRange(rgn,provider.device->props.nonCoherentAtomSize,shift);

  std::lock_guard<std::mutex> g(page.page->mmapSync);
  if(vkMapMemory(dev,page.page->memory,rgn.offset,rgn.size,0,&data)!=VK_SUCCESS)
    return false;

  data = reinterpret_cast<uint8_t*>(data) + shift;
  std::memcpy(data, mem, size);

  vkFlushMappedMemoryRanges(dev,1,&rgn);
  vkUnmapMemory(dev,page.page->memory);
  return true;
  }

bool VAllocator::read(VBuffer &src, void *mem, size_t offset, size_t size) {
  auto& page = src.page;
  void* data = nullptr;

  VkMappedMemoryRange rgn={};
  rgn.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  rgn.memory = page.page->memory;
  rgn.offset = page.offset+offset;
  rgn.size   = size;
  size_t shift = 0;
  alignRange(rgn,provider.device->props.nonCoherentAtomSize,shift);

  std::lock_guard<std::mutex> g(page.page->mmapSync);
  if(vkMapMemory(dev,page.page->memory,rgn.offset,rgn.size,0,&data)!=VK_SUCCESS)
    return false;
  vkInvalidateMappedMemoryRanges(dev,1,&rgn);

  data = reinterpret_cast<uint8_t*>(data)+shift;
  std::memcpy(mem,data,size);

  vkUnmapMemory(dev,page.page->memory);
  return true;
  }

VkSampler VAllocator::updateSampler(const Tempest::Sampler &s) {
  return samplers.get(s);
  }

bool VAllocator::commit(VkDeviceMemory dmem, std::mutex &mmapSync, VkBuffer dest, size_t pageOffset, const void* mem, size_t size) {
  VkMappedMemoryRange rgn={};
  rgn.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  rgn.memory = dmem;
  rgn.offset = pageOffset;
  rgn.size   = size;
  size_t shift = 0;
  alignRange(rgn,provider.device->props.nonCoherentAtomSize,shift);

  std::lock_guard<std::mutex> g(mmapSync); // on practice bind requires external sync
  if(vkBindBufferMemory(dev,dest,dmem,pageOffset)!=VK_SUCCESS)
    return false;
  if(mem!=nullptr) {
    void* data=nullptr;
    if(vkMapMemory(dev,dmem,pageOffset,rgn.size,0,&data)!=VK_SUCCESS)
      return false;
    data = reinterpret_cast<uint8_t*>(data)+shift;
    std::memcpy(data, mem, size);
    vkFlushMappedMemoryRanges(dev,1,&rgn);
    vkUnmapMemory(dev,dmem);
    }

  return true;
  }

bool VAllocator::commit(VkDeviceMemory dmem, std::mutex& mmapSync, VkImage dest, size_t offset) {
  std::lock_guard<std::mutex> g(mmapSync); // on practice bind requires external sync
  return vkBindImageMemory(dev, dest, dmem, offset)==VkResult::VK_SUCCESS;
  }

#endif
