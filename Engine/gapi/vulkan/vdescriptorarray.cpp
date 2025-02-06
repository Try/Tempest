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

static VkImageLayout toWriteLayout(VTexture& tex) {
  if(nativeIsDepthFormat(tex.format))
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  if(tex.isStorageImage)
    return VK_IMAGE_LAYOUT_GENERAL;
  return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

VDescriptorArray::VDescriptorArray(VDevice& device, VPipelineLay& vlay)
  :device(device), lay(&vlay), uav(vlay.lay.size()) {
  if(vlay.runtimeSized) {
    runtimeArrays.resize(vlay.lay.size());
    for(size_t i=0; i<vlay.lay.size(); ++i) {
      auto& lx = vlay.lay[i];
      runtimeArrays[i] = lx.arraySize;
      }
    } else {
    std::lock_guard<Detail::SpinLock> guard(vlay.syncPool);
    for(auto& i:vlay.pool) {
      if(i.freeCount==0)
        continue;
      impl = allocDescSet(i.impl,lay.handler->impl);
      if(impl!=VK_NULL_HANDLE) {
        pool=&i;
        pool->freeCount--;
        return;
        }
      }

    vlay.pool.emplace_back();
    auto& b = vlay.pool.back();
    b.impl  = allocPool(vlay);
    impl    = allocDescSet(b.impl,lay.handler->impl);
    if(impl==VK_NULL_HANDLE)
      throw std::bad_alloc();
    pool = &b;
    pool->freeCount--;
    }
  }

VDescriptorArray::~VDescriptorArray() {
  if(impl==VK_NULL_HANDLE)
    return;

  VkDevice dev = device.device.impl;
  if(dedicatedPool!=VK_NULL_HANDLE) {
    vkFreeDescriptorSets(dev,dedicatedPool,1,&impl);
    vkDestroyDescriptorPool(dev,dedicatedPool,nullptr);
    } else {
    Detail::VPipelineLay* layImpl = lay.handler;
    std::lock_guard<Detail::SpinLock> guard(layImpl->syncPool);

    vkFreeDescriptorSets(dev,pool->impl,1,&impl);
    pool->freeCount++;
    }
  }

VkDescriptorPool VDescriptorArray::allocPool(const VPipelineLay& lay) {
  VkDescriptorPoolSize poolSize[int(ShaderReflection::Class::Count)] = {};
  size_t               pSize=0;
  uint32_t             maxSets = VPipelineLay::POOL_SIZE;
  if(lay.runtimeSized)
    maxSets = 1;

  for(size_t i=0;i<lay.lay.size();++i) {
    if(lay.lay[i].stage==Tempest::Detail::ShaderReflection::None)
      continue;
    auto     cls = lay.lay[i].cls;
    uint32_t cnt = lay.lay[i].arraySize;
    if(lay.lay[i].runtimeSized)
      cnt = std::max<uint32_t>(1, runtimeArrays[i]);
    switch(cls) {
      case ShaderReflection::Ubo:     addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);             break;
      case ShaderReflection::Texture: addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);     break;
      case ShaderReflection::Image:   addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);              break;
      case ShaderReflection::Sampler: addPoolSize(poolSize,pSize,cnt,VK_DESCRIPTOR_TYPE_SAMPLER);                    break;
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
    i.descriptorCount *= maxSets;

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = maxSets;
  poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.poolSizeCount = uint32_t(pSize);
  poolInfo.pPoolSizes    = poolSize;

  if(lay.runtimeSized)
    poolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

  VkDevice dev = device.device.impl;
  VkDescriptorPool ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorPool(dev,&poolInfo,nullptr,&ret));
  return ret;
  }

VkDescriptorSet VDescriptorArray::allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay) {
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &lay;

  VkDevice        dev  = device.device.impl;
  VkDescriptorSet desc = VK_NULL_HANDLE;
  VkResult        ret  = vkAllocateDescriptorSets(dev,&allocInfo,&desc);
  if(ret==VK_ERROR_FRAGMENTED_POOL)
    return VK_NULL_HANDLE;
  if(ret!=VK_SUCCESS)
    return VK_NULL_HANDLE;
  return desc;
  }

void VDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture* t, const Sampler& smp, uint32_t mipLevel) {
  VkDevice  dev = device.device.impl;
  VTexture& tex = *reinterpret_cast<VTexture*>(t);
  if(impl==VK_NULL_HANDLE) {
    reallocSet(id, 0);
    }

  auto& lx = lay.handler->lay[id];
  VkDescriptorImageInfo imageInfo = {};
  if(lx.cls==ShaderReflection::Texture) {
    auto sx = smp;
    if(!tex.isFilterable) {
      sx.setFiltration(Filter::Nearest);
      sx.anisotropic = false;
      }
    imageInfo.sampler   = device.allocator.updateSampler(sx);
    imageInfo.imageView = tex.view(smp.mapping,mipLevel,lx.is3DImage);
    } else {
    imageInfo.imageView = tex.view(ComponentMapping(),mipLevel,lx.is3DImage);
    }
  imageInfo.imageLayout = toWriteLayout(tex);

  VkWriteDescriptorSet  descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = impl;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = nativeFormat(lx.cls);
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo      = &imageInfo;

  vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, nullptr);

  uav[id].tex     = t;
  uavUsage.durty |= (tex.nonUniqId!=0);
  }

void VDescriptorArray::set(size_t id, Tempest::AbstractGraphicsApi::Buffer* b, size_t offset) {
  VkDevice dev  = device.device.impl;
  VBuffer* buf  = reinterpret_cast<VBuffer*>(b);
  auto&    slot = lay.handler->lay[id];
  if(impl==VK_NULL_HANDLE) {
    reallocSet(id, 0);
    }

  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = buf!=nullptr ? buf->impl : VK_NULL_HANDLE;
  bufferInfo.offset = offset;
  bufferInfo.range  = (buf!=nullptr && slot.varByteSize==0) ? slot.byteSize : VK_WHOLE_SIZE;

  if(!device.props.hasRobustness2 && buf==nullptr) {
    //NOTE1: use of null-handle is not allowed, unless VK_EXT_robustness2
    //NOTE2: sizeof 1 is rouned up in shader; and sizeof 0 is illegal but harmless(hepefully)
    bufferInfo.buffer = device.dummySsbo().impl;
    bufferInfo.offset = offset;
    bufferInfo.range  = 0;
    }

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = impl;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = nativeFormat(lay.handler->lay[id].cls);
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo     = &bufferInfo;

  vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, nullptr);

  uav[id].buf     = b;
  uavUsage.durty |= (buf!=nullptr && buf->nonUniqId!=0);
  }

void VDescriptorArray::set(size_t id, const Sampler& smp) {
  VkDevice dev = device.device.impl;
  VkDescriptorImageInfo imageInfo = {};
  imageInfo.sampler = device.allocator.updateSampler(smp);

  VkWriteDescriptorSet  descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = impl;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = nativeFormat(lay.handler->lay[id].cls);
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo      = &imageInfo;

  vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray::set(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) {
  VkDevice                dev    = device.device.impl;
  VAccelerationStructure* memory = reinterpret_cast<VAccelerationStructure*>(tlas);

  VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
  descriptorAccelerationStructureInfo.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
  descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
  descriptorAccelerationStructureInfo.pAccelerationStructures    = &memory->impl;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.pNext           = &descriptorAccelerationStructureInfo;
  descriptorWrite.dstSet          = impl;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  descriptorWrite.descriptorCount = 1;

  vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, nullptr);
  // uavUsage.durty = true;
  }

void VDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture** t, size_t cnt, const Sampler& smp, uint32_t mipLevel) {
  VkDevice dev = device.device.impl;
  auto&    l   = lay.handler->lay[id];
  if(l.runtimeSized) {
    const uint32_t rSz = ((cnt+ALLOC_GRANULARITY-1u) & (~(ALLOC_GRANULARITY-1u)));
    if(runtimeArrays[id]!=rSz) {
      auto oldSz = runtimeArrays[id];
      try {
        runtimeArrays[id] = rSz;
        reallocSet(id, oldSz);
        }
      catch(...) {
        runtimeArrays[id] = oldSz;
        throw;
        }
      }
    }

  if(cnt==0)
    return;

  if(!l.runtimeSized) {
    // GLSL compiller issue: bindless array might be demote to bindfull one
    cnt = std::min<uint32_t>(l.arraySize, uint32_t(cnt));
    }

  // 16 is non-bindless limit
  SmallArray<VkDescriptorImageInfo,16> imageInfo(cnt);
  for(size_t i=0; i<cnt; ++i) {
    VTexture& tex = *reinterpret_cast<VTexture*>(t[i]);
    auto sx = smp;
    if(!tex.isFilterable)
      sx.setFiltration(Filter::Nearest);
    imageInfo[i].imageLayout = toWriteLayout(tex);
    imageInfo[i].imageView   = tex.view(smp.mapping,uint32_t(-1),l.is3DImage);
    imageInfo[i].sampler     = device.allocator.updateSampler(sx);
    // TODO: support mutable textures in bindings
    assert(tex.nonUniqId==0);
    }

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = impl;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = nativeFormat(l.cls);
  descriptorWrite.descriptorCount = uint32_t(cnt);
  descriptorWrite.pImageInfo      = imageInfo.get();

  vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer** b, size_t cnt) {
  VkDevice dev = device.device.impl;
  auto&    l   = lay.handler->lay[id];
  if(l.runtimeSized) {
    const uint32_t rSz = ((cnt+ALLOC_GRANULARITY-1u) & (~(ALLOC_GRANULARITY-1u)));
    if(runtimeArrays[id]!=rSz) {
      auto oldSz = runtimeArrays[id];
      try {
        runtimeArrays[id] = rSz;
        reallocSet(id, oldSz);
        }
      catch(...) {
        runtimeArrays[id] = oldSz;
        throw;
        }
      }
    }

  if(cnt==0)
    return;

  if(!l.runtimeSized) {
    // GLSL compiller issue: bindless array might be demote to bindfull one
    cnt = std::min<uint32_t>(l.arraySize, uint32_t(cnt));
    }

  SmallArray<VkDescriptorBufferInfo,32> bufInfo(cnt);
  for(size_t i=0; i<cnt; ++i) {
    VBuffer* buf = reinterpret_cast<VBuffer*>(b[i]);
    bufInfo[i].buffer = buf!=nullptr ? buf->impl : VK_NULL_HANDLE;
    bufInfo[i].offset = 0;
    bufInfo[i].range  = VK_WHOLE_SIZE;
    if(!device.props.hasRobustness2 && buf==nullptr) {
      bufInfo[i].buffer = device.dummySsbo().impl;
      bufInfo[i].offset = 0;
      bufInfo[i].range  = 0;
      }
    // assert(buf->nonUniqId==0);
    }

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = impl;
  descriptorWrite.dstBinding      = uint32_t(id);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorWrite.descriptorCount = uint32_t(cnt);
  descriptorWrite.pBufferInfo     = bufInfo.get();

  vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, nullptr);
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

bool VDescriptorArray::isRuntimeSized() const {
  return runtimeArrays.size()>0;
  }

void VDescriptorArray::addPoolSize(VkDescriptorPoolSize *p, size_t &sz, uint32_t cnt, VkDescriptorType elt) {
  for(size_t i=0; i<sz; ++i){
    if(p[i].type==elt) {
      p[i].descriptorCount += cnt;
      return;
      }
    }
  p[sz].type            = elt;
  p[sz].descriptorCount = cnt;
  sz++;
  }

void VDescriptorArray::reallocSet(size_t id, uint32_t oldRuntimeSz) {
  auto  dev      = device.device.impl;
  auto& lay      = *this->lay.handler;
  auto  prevPool = dedicatedPool;
  auto  prevDesc = impl;

  auto lx = lay.create(runtimeArrays);

  dedicatedPool = allocPool(lay);
  if(dedicatedPool==VK_NULL_HANDLE) {
    dedicatedPool = prevPool;
    throw std::bad_alloc();
    }

  impl = allocDescSet(dedicatedPool,lx.dLay);
  if(impl==VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(dev,dedicatedPool,nullptr);

    dedicatedPool    = prevPool;
    impl             = prevDesc;
    throw std::bad_alloc();
    }

  dedicatedLayout = lx.pLay;
  if(prevDesc==VK_NULL_HANDLE)
    return;

  SmallArray<VkCopyDescriptorSet,16> cpy(lay.lay.size());
  uint32_t                           cnt = 0;
  for(size_t i=0; i<lay.lay.size(); ++i) {
    VkCopyDescriptorSet& cx = cpy[cnt];
    auto&                lx = lay.lay[i];
    cx.sType           = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    cx.pNext           = nullptr;
    cx.srcSet          = prevDesc;
    cx.srcBinding      = uint32_t(i);
    cx.srcArrayElement = 0;

    cx.dstSet          = impl;
    cx.dstBinding      = uint32_t(i);
    cx.dstArrayElement = 0;

    cx.descriptorCount = lx.runtimeSized ? runtimeArrays[i] : lx.arraySize;
    if(i==id)
      cx.descriptorCount = std::min(oldRuntimeSz, cx.descriptorCount);

    if(cx.descriptorCount>0)
      ++cnt;
    }
  vkUpdateDescriptorSets(dev,0,nullptr,uint32_t(cnt),cpy.get());

  if(prevDesc!=VK_NULL_HANDLE)
    vkFreeDescriptorSets(dev,prevPool,1,&prevDesc);
  if(prevPool!=VK_NULL_HANDLE)
    vkDestroyDescriptorPool(dev,prevPool,nullptr);
  }

///////////

VDescriptorArray2::VDescriptorArray2(VDevice &dev, AbstractGraphicsApi::Texture **tex, size_t cnt, uint32_t mipLevel, const Sampler &smp)
  : dev(dev), cnt(cnt) {
  const auto& lay = dev.bindlessArrayLayout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  alloc(lay.impl, dev, cnt);
  populate(dev, tex, cnt, mipLevel, &smp);
  }

VDescriptorArray2::VDescriptorArray2(VDevice &dev, AbstractGraphicsApi::Texture **tex, size_t cnt, uint32_t mipLevel)
  : dev(dev), cnt(cnt) {
  const auto& lay = dev.bindlessArrayLayout(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
  alloc(lay.impl, dev, cnt);
  populate(dev, tex, cnt, mipLevel, nullptr);
  }

VDescriptorArray2::VDescriptorArray2(VDevice &dev, AbstractGraphicsApi::Buffer **buf, size_t cnt)
  : dev(dev), cnt(cnt) {
  const auto& lay = dev.bindlessArrayLayout(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  alloc(lay.impl, dev, cnt);
  populate(dev, buf, cnt);
  }

VDescriptorArray2::~VDescriptorArray2() {
  dev.bindless.notifyDestroy(this);
  if(pool!=VK_NULL_HANDLE)
    vkDestroyDescriptorPool(dev.device.impl, pool, nullptr);
  }

size_t VDescriptorArray2::size() const {
  return cnt;
  }

void VDescriptorArray2::alloc(VkDescriptorSetLayout lay, VDevice &dev, size_t cnt) {
  VkDescriptorPoolSize       poolSize = {};
  poolSize.type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  poolSize.descriptorCount = uint32_t(cnt);

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = 1;
  poolInfo.flags         = 0; // VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_EXT;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes    = &poolSize;
  poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  vkAssert(vkCreateDescriptorPool(dev.device.impl, &poolInfo, nullptr, &pool));

  uint32_t cnt32 = uint32_t(cnt);
  VkDescriptorSetVariableDescriptorCountAllocateInfo vinfo = {};
  vinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
  vinfo.descriptorSetCount = 1;
  vinfo.pDescriptorCounts  = &cnt32;

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.pNext              = &vinfo;
  allocInfo.descriptorPool     = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &lay;
  vkAssert(vkAllocateDescriptorSets(dev.device.impl,&allocInfo,&dset));
  }

void VDescriptorArray2::populate(VDevice &dev, AbstractGraphicsApi::Texture **t, size_t cnt, uint32_t mipLevel, const Sampler* smp) {
  const bool is3DImage = false; //TODO

  // 16 is non-bindless limit
  SmallArray<VkDescriptorImageInfo,16> imageInfo(cnt);
  for(size_t i=0; i<cnt; ++i) {
    VTexture& tex = *reinterpret_cast<VTexture*>(t[i]);
    imageInfo[i].imageLayout = toWriteLayout(tex);
    imageInfo[i].imageView   = tex.view(ComponentMapping(), uint32_t(-1), is3DImage);
    imageInfo[i].sampler     = smp!=nullptr ? dev.allocator.updateSampler(*smp) : VK_NULL_HANDLE;
    // TODO: support mutable textures in bindless
    assert(tex.nonUniqId==0);
    }

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = dset;
  descriptorWrite.dstBinding      = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = smp!=nullptr ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptorWrite.descriptorCount = uint32_t(cnt);
  descriptorWrite.pImageInfo      = imageInfo.get();

  vkUpdateDescriptorSets(dev.device.impl, 1, &descriptorWrite, 0, nullptr);
  }

void VDescriptorArray2::populate(VDevice &dev, AbstractGraphicsApi::Buffer **b, size_t cnt) {
  // 16 is non-bindless limit
  SmallArray<VkDescriptorBufferInfo,16> bufInfo(cnt);
  for(size_t i=0; i<cnt; ++i) {
    VBuffer* buf = reinterpret_cast<VBuffer*>(b[i]);
    bufInfo[i].buffer = buf!=nullptr ? buf->impl : VK_NULL_HANDLE;
    bufInfo[i].offset = 0;
    bufInfo[i].range  = VK_WHOLE_SIZE;
    if(!dev.props.hasRobustness2 && buf==nullptr) {
      bufInfo[i].buffer = dev.dummySsbo().impl;
      bufInfo[i].offset = 0;
      bufInfo[i].range  = 0;
      }
    // assert(buf->nonUniqId==0);
    }

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet          = dset;
  descriptorWrite.dstBinding      = uint32_t(0);
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorWrite.descriptorCount = uint32_t(cnt);
  descriptorWrite.pBufferInfo     = bufInfo.get();

  vkUpdateDescriptorSets(dev.device.impl, 1, &descriptorWrite, 0, nullptr);
  }

#endif
