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

void VAllocator::setDevice(VDevice &dev) {
  device          = dev.device;
  provider.device = &dev;
  samplers.setDevice(dev);
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

  vkAssert(vkAllocateMemory(device->device,&vk_memoryAllocateInfo,nullptr,&memory));
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
                          MemUsage usage, BufferFlags bufFlg) {
  VBuffer ret;
  ret.alloc = this;

  VkBufferCreateInfo createInfo={};
  createInfo.sType                 = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  createInfo.pNext                 = nullptr;
  createInfo.flags                 = 0;
  createInfo.size                  = count*alignedSz;
  createInfo.sharingMode           = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
  createInfo.queueFamilyIndexCount = 0;
  createInfo.pQueueFamilyIndices   = nullptr;

  if(bool(usage & MemUsage::TransferSrc))
    createInfo.usage |= VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  if(bool(usage & MemUsage::TransferDst))
    createInfo.usage |= VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if(bool(usage & MemUsage::UniformBit))
    createInfo.usage |= VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  if(bool(usage & MemUsage::VertexBuffer))
    createInfo.usage |= VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if(bool(usage & MemUsage::IndexBuffer))
    createInfo.usage |= VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

  vkAssert(vkCreateBuffer(device,&createInfo,nullptr,&ret.impl));

  MemRequirements memRq={};
  getMemoryRequirements(memRq,ret.impl);

  uint32_t props=0;
  if(bool(bufFlg&BufferFlags::Staging))
    props|=VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    //props|=(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);// to avoid vkFlushMappedMemoryRanges
  if(bool(bufFlg&BufferFlags::Static))
    props|=VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  if(bool(bufFlg&BufferFlags::Dynamic))
    props|=(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  VDevice::MemIndex memId = provider.device->memoryTypeIndex(memRq.memoryTypeBits,VkMemoryPropertyFlagBits(props),VK_IMAGE_TILING_LINEAR);
  ret.page = allocMemory(memRq,memId.heapId,memId.typeId);

  if(!ret.page.page)
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);

  if(!commit(ret.page.page->memory,ret.page.page->mmapSync,ret.impl,ret.page.offset,
             mem,count,size,alignedSz)) {
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }
  return ret;
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
  imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  if(mip>1) {
    // TODO: test VK_FORMAT_FEATURE_BLIT_DST_BIT
    imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

  vkAssert(vkCreateImage(device, &imageInfo, nullptr, &ret.impl));

  MemRequirements memRq={};
  getImgMemoryRequirements(memRq,ret.impl);

  VDevice::MemIndex memId = provider.device->memoryTypeIndex(memRq.memoryTypeBits,VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,VK_IMAGE_TILING_OPTIMAL);
  ret.page = allocMemory(memRq,memId.heapId,memId.typeId);

  if(!ret.page.page) {
    ret.alloc = nullptr;
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }
  if(!commit(ret.page.page->memory,ret.page.page->mmapSync,ret.impl,ret.page.offset)) {
    ret.alloc = nullptr;
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }

  auto s = samplers.get(mip);
  ret.createView(device,imageInfo.format,s,mip);
  return ret;
  }

VTexture VAllocator::alloc(const uint32_t w, const uint32_t h, const uint32_t mip, TextureFormat frm) {
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
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.format        = nativeFormat(frm);

  vkAssert(vkCreateImage(device, &imageInfo, nullptr, &ret.impl));

  MemRequirements memRq={};
  getImgMemoryRequirements(memRq,ret.impl);

  VDevice::MemIndex memId = provider.device->memoryTypeIndex(memRq.memoryTypeBits,VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,VK_IMAGE_TILING_OPTIMAL);
  ret.page = allocMemory(memRq,memId.heapId,memId.typeId);

  if(!ret.page.page) {
    ret.alloc = nullptr;
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }
  if(!commit(ret.page.page->memory,ret.page.page->mmapSync,ret.impl,ret.page.offset)) {
    ret.alloc = nullptr;
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
    }

  auto s = samplers.get(mip);
  ret.createView(device,imageInfo.format,s,mip);
  return ret;
  }

void VAllocator::free(VBuffer &buf) {
  if(buf.impl!=VK_NULL_HANDLE) {
    vkDestroyBuffer (device,buf.impl,nullptr);
    }
  if(buf.page.page!=nullptr)
    allocator.free(buf.page);
  }

void VAllocator::free(VTexture &buf) {
  if(buf.sampler!=VK_NULL_HANDLE) {
    vkDeviceWaitIdle  (device);
    samplers.free(buf.sampler);
    vkDestroyImageView(device,buf.view,nullptr);
    vkDestroyImage    (device,buf.impl,nullptr);
    }
  else if(buf.view!=VK_NULL_HANDLE) {
    vkDeviceWaitIdle  (device);
    vkDestroyImageView(device,buf.view,nullptr);
    vkDestroyImage    (device,buf.impl,nullptr);
    }
  else if(buf.impl!=VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device);
    vkDestroyImage  (device,buf.impl,nullptr);
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

    provider.device->vkGetBufferMemoryRequirements2(device,&bufInfo,&memReq2);

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
  vkGetBufferMemoryRequirements(device,buf,&memRq);
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

    provider.device->vkGetImageMemoryRequirements2(device,&bufInfo,&memReq2);

    out.size           = size_t(memReq2.memoryRequirements.size);
    out.alignment      = size_t(memReq2.memoryRequirements.alignment);
    out.memoryTypeBits = memReq2.memoryRequirements.memoryTypeBits;
    out.dedicated =
        (memDedicatedRq.requiresDedicatedAllocation != VK_FALSE) ||
        (memDedicatedRq.prefersDedicatedAllocation  != VK_FALSE);
    return;
    }

  VkMemoryRequirements memRq={};
  vkGetImageMemoryRequirements(device,img,&memRq);
  out.size           = size_t(memRq.size);
  out.alignment      = size_t(memRq.alignment);
  out.memoryTypeBits = memRq.memoryTypeBits;
  }

VAllocator::Allocation VAllocator::allocMemory(const VAllocator::MemRequirements& memRq, const size_t heapId, const size_t typeId) {
  const size_t align = LCM(memRq.alignment,provider.device->props.nonCoherentAtomSize);
  Allocation ret;
  if(memRq.dedicated) {
    ret = allocator.dedicatedAlloc(memRq.size,align,heapId,typeId);
    if(!ret.page && !memRq.dedicatedRq)
      ret = allocator.alloc(memRq.size,align,heapId,typeId);
    } else {
    ret = allocator.alloc(memRq.size,align,heapId,typeId);
    }
  return ret;
  }

bool VAllocator::update(VBuffer &dest, const void *mem,
                        size_t offset, size_t count, size_t size, size_t alignedSz) {
  auto& page = dest.page;
  void* data = nullptr;

  std::lock_guard<std::mutex> g(page.page->mmapSync);
  if(vkMapMemory(device,page.page->memory,
                 page.offset+offset*alignedSz,count*alignedSz,0,&data)!=VkResult::VK_SUCCESS)
    return false;

  copyUpsample(mem,data,count,size,alignedSz);

  VkMappedMemoryRange rgn={};
  rgn.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  rgn.memory = page.page->memory;
  rgn.offset = page.offset+offset;
  rgn.size   = (size%provider.device->props.nonCoherentAtomSize==0) ? size : VK_WHOLE_SIZE;
  vkFlushMappedMemoryRanges(device,1,&rgn);

  vkUnmapMemory(device,page.page->memory);
  return true;
  }

bool VAllocator::read(VBuffer &src, void *mem, size_t offset, size_t size) {
  auto& page = src.page;
  void* data = nullptr;

  std::lock_guard<std::mutex> g(page.page->mmapSync);
  if(vkMapMemory(device,page.page->memory,page.offset+offset,size,0,&data)!=VkResult::VK_SUCCESS)
    return false;

  VkMappedMemoryRange rgn={};
  rgn.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  rgn.memory = page.page->memory;
  rgn.offset = page.offset+offset; //TODO: nonCoherentAtomSize
  rgn.size   = size;
  vkInvalidateMappedMemoryRanges(device,1,&rgn);

  std::memcpy(mem,data,size);

  vkUnmapMemory(device,page.page->memory);
  return true;
  }

void VAllocator::updateSampler(VkSampler &smp, const Tempest::Sampler2d &s,uint32_t mipCount) {
  auto ns = samplers.get(s,mipCount);
  samplers.free(smp);
  smp = ns;
  }

bool VAllocator::commit(VkDeviceMemory dev, std::mutex &mmapSync, VkBuffer dest,
                        size_t pageOffset, const void* mem,  size_t count, size_t size, size_t alignedSz) {
  std::lock_guard<std::mutex> g(mmapSync); // on practice bind requires external sync
  if(vkBindBufferMemory(device,dest,dev,pageOffset)!=VK_SUCCESS)
    return false;
  if(mem!=nullptr) {
    void* data=nullptr;
    if(vkMapMemory(device,dev,pageOffset,size,0,&data)!=VK_SUCCESS)
      return false;
    copyUpsample(mem,data,count,size,alignedSz);
    vkUnmapMemory(device,dev);
    }

  return true;
  }

bool VAllocator::commit(VkDeviceMemory dev, std::mutex& mmapSync, VkImage dest, size_t offset) {
  std::lock_guard<std::mutex> g(mmapSync); // on practice bind requires external sync
  return vkBindImageMemory(device, dest, dev, offset)==VkResult::VK_SUCCESS;
  }
