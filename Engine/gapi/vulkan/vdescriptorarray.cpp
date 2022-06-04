#if defined(TEMPEST_BUILD_VULKAN)

#include "vbuffer.h"

#include <Tempest/PipelineLayout>

#include "vdevice.h"
#include "vdescriptorarray.h"
#include "vtexture.h"
#include "vaccelerationstructure.h"
#include "utility/smallarray.h"

using namespace Tempest;
using namespace Tempest::Detail;

static VkDescriptorType toWriteType(ShaderReflection::Class c) {
  switch(c) {
    case ShaderReflection::Ubo:     return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case ShaderReflection::Texture: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case ShaderReflection::SsboR:   return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderReflection::SsboRW:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderReflection::ImgR:    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case ShaderReflection::ImgRW:   return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case ShaderReflection::Tlas:    return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    case ShaderReflection::Push:    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    case ShaderReflection::Count:   return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  }

VDescriptorArray::VDescriptorArray(VkDevice device, VPipelineLay& vlay)
  :device(device), lay(&vlay), uav(vlay.lay.size()) {

  if(!vlay.runtimeSized) {
    std::lock_guard<Detail::SpinLock> guard(vlay.sync);
    for(auto& i:vlay.pool){
      if(i.freeCount==0)
        continue;
      desc = allocDescSet(i.impl,lay.handler->impl);
      if(desc!=VK_NULL_HANDLE) {
        pool=&i;
        pool->freeCount--;
        return;
        }
      }

    vlay.pool.emplace_back();
    auto& b = vlay.pool.back();
    b.impl  = allocPool(vlay,VPipelineLay::POOL_SIZE);
    desc    = allocDescSet(b.impl,lay.handler->impl);
    if(desc==VK_NULL_HANDLE)
      throw std::bad_alloc();
    pool = &b;
    pool->freeCount--;
    }
  }

VDescriptorArray::~VDescriptorArray() {
  if(desc==VK_NULL_HANDLE)
    return;
  if(dedicatedPool!=VK_NULL_HANDLE) {
    vkFreeDescriptorSets(device,dedicatedPool,1,&desc);
    vkDestroyDescriptorPool(device,dedicatedPool,nullptr);
    vkDestroyDescriptorSetLayout(device,dedicatedLayout,nullptr);
    } else {
    Detail::VPipelineLay* layImpl = lay.handler;
    std::lock_guard<Detail::SpinLock> guard(layImpl->sync);

    vkFreeDescriptorSets(device,pool->impl,1,&desc);
    pool->freeCount++;
    }
  }

VkDescriptorPool VDescriptorArray::allocPool(const VPipelineLay& lay, size_t size) {
  VkDescriptorPoolSize poolSize[int(ShaderReflection::Class::Count)] = {};
  size_t               pSize=0;

  for(size_t i=0;i<lay.lay.size();++i){
    auto     cls = lay.lay[i].cls;
    uint32_t cnt = 1;
    if(lay.lay[i].runtimeSized)
      cnt = runtimeArraySz;
    switch(cls) {
      case ShaderReflection::Ubo:     addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);             break;
      case ShaderReflection::Texture: addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);     break;
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW:  addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);             break;
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW:   addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);              break;
      case ShaderReflection::Tlas:    addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR); break;
      case ShaderReflection::Push:    break;
      case ShaderReflection::Count:   break;
      }
    }

  for(auto& i:poolSize)
    i.descriptorCount*=uint32_t(size);

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = uint32_t(size);
  poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.poolSizeCount = uint32_t(pSize);
  poolInfo.pPoolSizes    = poolSize;

  VkDescriptorPool ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorPool(device,&poolInfo,nullptr,&ret));
  return ret;
  }

VkDescriptorSet VDescriptorArray::allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay) {
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &lay;

  VkDescriptorSet desc = VK_NULL_HANDLE;
  VkResult ret=vkAllocateDescriptorSets(device,&allocInfo,&desc);
  if(ret==VK_ERROR_FRAGMENTED_POOL)
    return VK_NULL_HANDLE;
  if(ret!=VK_SUCCESS)
    return VK_NULL_HANDLE;
  return desc;
  }

void VDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture* t, const Sampler2d& smp, uint32_t mipLevel) {
  VTexture* tex=reinterpret_cast<VTexture*>(t);

  VkDescriptorImageInfo imageInfo = {};

  VkWriteDescriptorSet  descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = toWriteType(lay.handler->lay[id].cls);
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo      = &imageInfo;

  if(descriptorWrite.descriptorType==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
    imageInfo.sampler   = tex->alloc->updateSampler(smp);
    imageInfo.imageView = tex->view(device,smp.mapping,mipLevel);
    } else {
    if(mipLevel==uint32_t(-1))
      mipLevel = 0;
    imageInfo.imageView = tex->view(device,ComponentMapping(),mipLevel);
    }

  if(nativeIsDepthFormat(tex->format))
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  else if(tex->isStorageImage)
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  else
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

  uav[id].tex     = t;
  uavUsage.durty |= (tex->nonUniqId!=0);
  }

void VDescriptorArray::set(size_t id, Tempest::AbstractGraphicsApi::Buffer* b, size_t offset) {
  VBuffer* buf  = reinterpret_cast<VBuffer*>(b);
  auto&    slot = lay.handler->lay[id];

  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = buf->impl;
  bufferInfo.offset = offset;
  bufferInfo.range  = slot.size;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = toWriteType(lay.handler->lay[id].cls);
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo     = &bufferInfo;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

  uav[id].buf     = b;
  uavUsage.durty |= (buf->nonUniqId!=0);
  }

void VDescriptorArray::setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) {
  VAccelerationStructure* memory=reinterpret_cast<VAccelerationStructure*>(tlas);

  VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
  descriptorAccelerationStructureInfo.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
  descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
  descriptorAccelerationStructureInfo.pAccelerationStructures    = &memory->impl;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.pNext           = &descriptorAccelerationStructureInfo;
  descriptorWrite.dstSet          = desc;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  descriptorWrite.descriptorCount = 1;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
  // uavUsage.durty = true;
  }

void VDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture** t, size_t cnt, const Sampler2d& smp) {
  constexpr uint32_t granularity = VPipelineLay::MAX_BINDLESS;
  uint32_t rSz = ((cnt+granularity-1u) & (~(granularity-1u)));
  if(runtimeArraySz!=rSz) {
    runtimeArraySz = rSz;
    reallocSet();
    }

  SmallArray<VkDescriptorImageInfo,32> imageInfo(cnt);
  for(size_t i=0; i<cnt; ++i) {
    VTexture* tex=reinterpret_cast<VTexture*>(t[i]);
    imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo[i].imageView   = tex->view(device,smp.mapping,uint32_t(-1));
    imageInfo[i].sampler     = tex->alloc->updateSampler(smp);
    }

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = desc;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = uint32_t(cnt);
  descriptorWrite.pImageInfo      = imageInfo.get();

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray::ssboBarriers(ResourceState& res, PipelineStage st) {
  auto& lay = this->lay.handler->lay;
  if(T_UNLIKELY(uavUsage.durty)) {
    uavUsage.read  = NonUniqResId::I_None;
    uavUsage.write = NonUniqResId::I_None;
    for(size_t i=0; i<lay.size(); ++i) {
      NonUniqResId id = NonUniqResId::I_None;
      if(uav[i].buf!=nullptr)
        id = reinterpret_cast<VBuffer*> (uav[i].buf)->nonUniqId;
      if(uav[i].tex!=nullptr)
        id = reinterpret_cast<VTexture*>(uav[i].tex)->nonUniqId;

      uavUsage.read |= id;
      if(lay[i].cls==ShaderReflection::ImgRW || lay[i].cls==ShaderReflection::SsboRW)
        uavUsage.write |= id;
      }
    uavUsage.durty = false;
    }
  res.onUavUsage(uavUsage,st);
  }

void VDescriptorArray::addPoolSize(VkDescriptorPoolSize *p, size_t &sz, uint32_t cnt, VkDescriptorType elt) {
  for(size_t i=0;i<sz;++i){
    if(p[i].type==elt) {
      p[i].descriptorCount += cnt;
      return;
      }
    }
  p[sz].type            = elt;
  p[sz].descriptorCount = cnt;
  sz++;
  }

void VDescriptorArray::reallocSet() {
  auto& lay      = *this->lay.handler;
  auto  prevLay  = dedicatedLayout;
  auto  prevPool = dedicatedPool;
  auto  prevDesc = desc;

  dedicatedLayout = lay.create(runtimeArraySz);
  if(dedicatedLayout==VK_NULL_HANDLE) {
    dedicatedLayout = prevLay;
    throw std::bad_alloc();
    }
  dedicatedPool = allocPool(lay,1);
  if(dedicatedPool==VK_NULL_HANDLE) {
    dedicatedLayout = prevLay;
    dedicatedPool   = prevPool;
    throw std::bad_alloc();
    }
  desc = allocDescSet(dedicatedPool,dedicatedLayout);
  if(desc==VK_NULL_HANDLE) {
    dedicatedLayout = prevLay;
    dedicatedPool   = prevPool;
    desc            = prevDesc;
    throw std::bad_alloc();
    }

  if(prevDesc==VK_NULL_HANDLE)
    return;

  SmallArray<VkCopyDescriptorSet,8> cpy(lay.lay.size());

  for(size_t i=0;i<lay.lay.size();++i) {
    VkCopyDescriptorSet& cx = cpy[i];
    auto&                lx = lay.lay[i];
    cx.sType           = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    cx.pNext           = nullptr;
    cx.srcSet          = prevDesc;
    cx.srcBinding      = uint32_t(i);
    cx.srcArrayElement = 0;

    cx.dstSet          = desc;
    cx.dstBinding      = uint32_t(i);
    cx.dstArrayElement = 0;

    cx.descriptorCount = lx.runtimeSized ? runtimeArraySz : 1;
    }
  vkUpdateDescriptorSets(device,0,nullptr,uint32_t(lay.lay.size()),cpy.get());

  if(prevDesc!=VK_NULL_HANDLE)
    vkFreeDescriptorSets(device,prevPool,1,&prevDesc);
  if(prevPool!=VK_NULL_HANDLE)
    vkDestroyDescriptorPool(device,prevPool,nullptr);
  if(prevLay!=VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(device,prevLay,nullptr);
  }

#endif
