#if defined(TEMPEST_BUILD_VULKAN)

#include "vallocator.h"

#include "vdevice.h"
#include "vbuffer.h"
#include "vtexture.h"

#include "exceptions/exception.h"

#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <thread>

#include "gapi/graphicsmemutils.h"

using namespace Tempest;
using namespace Tempest::Detail;

VAllocator::VAllocator() {
  }

void VAllocator::setDevice(VDevice &d) {
  dev             = d.device;
  provider.device = &d;
  samplers.setDevice(d);
  }

VDevice* VAllocator::device() {
  return provider.device;
  }

VAllocator::Provider::~Provider() {
  if(lastFree!=VK_NULL_HANDLE)
    vkFreeMemory(device->device,lastFree,nullptr);
  }

VAllocator::Provider::DeviceMemory VAllocator::Provider::alloc(size_t size,uint32_t typeId) {
  if(lastFree!=VK_NULL_HANDLE){
    if(lastType==typeId && lastSize==size){
      VkDeviceMemory memory=lastFree;
      lastFree=VK_NULL_HANDLE;
      return memory;
      }
    vkFreeMemory(device->device,lastFree,nullptr);
    lastFree=VK_NULL_HANDLE;
    }
  VkDeviceMemory memory=VK_NULL_HANDLE;

  VkMemoryAllocateInfo vk_memoryAllocateInfo;
  vk_memoryAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  vk_memoryAllocateInfo.pNext           = nullptr;
  vk_memoryAllocateInfo.allocationSize  = size;
  vk_memoryAllocateInfo.memoryTypeIndex = typeId;

  auto code = vkAllocateMemory(device->device,&vk_memoryAllocateInfo,nullptr,&memory);
  if(code!=VK_SUCCESS)
    return VK_NULL_HANDLE;
  return memory;
  }

void VAllocator::Provider::free(VAllocator::Provider::DeviceMemory m, size_t size, uint32_t typeId) {
  if(lastFree!=VK_NULL_HANDLE)
    vkFreeMemory(device->device,lastFree,nullptr);

  lastFree=m;
  lastSize=size;
  lastType=typeId;
  }

void VAllocator::Provider::freeLast() {
  if(lastFree!=VK_NULL_HANDLE)
    vkFreeMemory(device->device,lastFree,nullptr);
  lastFree=VK_NULL_HANDLE;
  }

static size_t GCD(size_t n1, size_t n2) {
  if(n1==n2)
    return n1;

  if(n1<n2) {
    size_t d = n2 - n1;
    return GCD(n1, d);
    } else {
    size_t d = n1 - n2;
    return GCD(n2, d);
    }
  }

static size_t LCM(size_t n1, size_t n2)  {
  return n1*n2 / GCD(n1, n2);
  }

VBuffer VAllocator::alloc(const void *mem, size_t count, size_t size, size_t alignedSz,
                          MemUsage usage, BufferHeap bufHeap) {
  VBuffer ret;
  ret.alloc = this;

  VkBufferCreateInfo createInfo={};
  createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  createInfo.pNext                 = nullptr;
  createInfo.flags                 = 0;
  createInfo.size                  = count*alignedSz;
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
    createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  if(MemUsage::StorageBuffer==(usage & MemUsage::StorageBuffer))
    createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  vkAssert(vkCreateBuffer(dev,&createInfo,nullptr,&ret.impl));

  MemRequirements memRq={};
  getMemoryRequirements(memRq,ret.impl);

  uint32_t props[2] = {};
  uint8_t  propsCnt = 1;
  if(bufHeap==BufferHeap::Upload && usage==MemUsage::UniformBuffer) {
    propsCnt = 2;
    props[0] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    props[1] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    //props[1]|=(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);// to avoid vkFlushMappedMemoryRanges
    }
  else if(bufHeap==BufferHeap::Upload)
    props[0] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  else if(bufHeap==BufferHeap::Device)
    props[0] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  else if(bufHeap==BufferHeap::Readback)
    props[0] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

  for(uint8_t i=0; i<propsCnt; ++i) {
    VDevice::MemIndex memId = provider.device->memoryTypeIndex(memRq.memoryTypeBits,VkMemoryPropertyFlagBits(props[i]),VK_IMAGE_TILING_LINEAR);
    if(memId.typeId==uint32_t(-1))
      continue;

    ret.page = allocMemory(memRq,memId.heapId,memId.typeId,(props[i]&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    if(!ret.page.page)
      continue;

    if(!commit(ret.page.page->memory,ret.page.page->mmapSync,ret.impl,ret.page.offset,
               mem,count,size,alignedSz)) {
      throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
      }
    return ret;
    }

  throw std::system_error(Tempest::GraphicsErrc::OutOfVideoMemory);
  }

VTexture VAllocator::alloc(const Pixmap& pm,uint32_t mip,VkFormat format) {
  VTexture ret;
  ret.alloc = this;

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

VTexture VAllocator::alloc(const uint32_t w, const uint32_t h, const uint32_t mip, TextureFormat frm, bool imgStorage) {
  VTexture ret;
  ret.alloc = this;

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType     = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width  = w;
  imageInfo.extent.height = h;
  imageInfo.extent.depth  = 1;
  imageInfo.mipLevels     = mip;
  imageInfo.arrayLayers   = 1;
  imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage         = isDepthFormat(frm) ?
        (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) :
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.format        = nativeFormat(frm);

  if(imgStorage)
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

  ret.format = imageInfo.format;
  ret.mipCnt = mip;
  ret.createViews(dev);
  return ret;
  }

void VAllocator::free(VBuffer &buf) {
  if(buf.impl!=VK_NULL_HANDLE)
    vkDestroyBuffer (dev,buf.impl,nullptr);

  if(buf.page.page!=nullptr)
    allocator.free(buf.page);
  }

void VAllocator::free(VTexture &buf) {
  if(buf.view!=VK_NULL_HANDLE) {
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

void VAllocator::alignRange(VkMappedMemoryRange& rgn, size_t nonCoherentAtomSize, size_t shift) {
  shift = rgn.offset%nonCoherentAtomSize;
  rgn.offset -= shift;
  rgn.size   += shift;

  if(rgn.size%nonCoherentAtomSize!=0)
    rgn.size += nonCoherentAtomSize-rgn.size%nonCoherentAtomSize;
  }

bool VAllocator::update(VBuffer &dest, const void *mem,
                        size_t offset, size_t count, size_t size, size_t alignedSz) {
  auto& page = dest.page;
  void* data = nullptr;

  VkMappedMemoryRange rgn={};
  rgn.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  rgn.memory = page.page->memory;
  rgn.offset = page.offset+offset*alignedSz;
  rgn.size   = count*alignedSz;

  size_t shift = 0;
  alignRange(rgn,provider.device->props.nonCoherentAtomSize,shift);

  std::lock_guard<std::mutex> g(page.page->mmapSync);
  if(vkMapMemory(dev,page.page->memory,rgn.offset,rgn.size,0,&data)!=VK_SUCCESS)
    return false;

  data = reinterpret_cast<uint8_t*>(data) + shift + offset*alignedSz;
  copyUpsample(mem,data,count,size,alignedSz);

  vkFlushMappedMemoryRanges(dev,1,&rgn);

  vkUnmapMemory(dev,page.page->memory);
  return true;
  }

bool VAllocator::read(VBuffer& src, void* mem, size_t offset, size_t count, size_t size, size_t alignedSz) {
  auto& page = src.page;
  void* data = nullptr;

  VkMappedMemoryRange rgn={};
  rgn.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  rgn.memory = page.page->memory;
  rgn.offset = page.offset+offset;
  rgn.size   = count*alignedSz;
  size_t shift = 0;
  alignRange(rgn,provider.device->props.nonCoherentAtomSize,shift);

  std::lock_guard<std::mutex> g(page.page->mmapSync);
  if(vkMapMemory(dev,page.page->memory,rgn.offset,rgn.size,0,&data)!=VK_SUCCESS)
    return false;
  vkInvalidateMappedMemoryRanges(dev,1,&rgn);

  data = reinterpret_cast<uint8_t*>(data)+shift;
  copyUpsample(data,mem,count,size,alignedSz);

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

void VAllocator::updateSampler(VkSampler &smp, const Tempest::Sampler2d &s, uint32_t mipCount) {
  auto ns = samplers.get(s,mipCount);
  samplers.free(smp);
  smp = ns;
  }

bool VAllocator::commit(VkDeviceMemory dmem, std::mutex &mmapSync, VkBuffer dest,
                        size_t pageOffset, const void* mem,  size_t count, size_t size, size_t alignedSz) {
  VkMappedMemoryRange rgn={};
  rgn.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  rgn.memory = dmem;
  rgn.offset = pageOffset;
  rgn.size   = count*alignedSz;
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
    copyUpsample(mem,data,count,size,alignedSz);
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
