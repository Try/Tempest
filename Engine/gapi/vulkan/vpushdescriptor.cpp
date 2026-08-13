#if defined(TEMPEST_BUILD_VULKAN)
#include "vpushdescriptor.h"

#include "gapi/vulkan/vaccelerationstructure.h"
#include "gapi/vulkan/vtexture.h"
#include "gapi/vulkan/vdescriptorarray.h"
#include "gapi/vulkan/vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

static VkImageLayout toWriteLayout(VTexture& tex) {
  if(nativeIsDepthFormat(tex.format))
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  if(tex.isStorageImage)
    return VK_IMAGE_LAYOUT_GENERAL;
  return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

static std::pair<uint32_t, uint32_t> numResources(const ShaderReflection::LayoutDesc& lay) {
  std::pair<uint32_t, uint32_t> ret;
  for(size_t i=0; i<MaxBindings; ++i) {
    if(((1u << i) & lay.array)!=0)
      continue;
    switch(lay.bindings[i]) {
      case ShaderReflection::Sampler:
        ret.second++;
        break;
      case ShaderReflection::Texture:
        ret.first++;
        ret.second++;
        break;
      case ShaderReflection::Ubo:
      case ShaderReflection::Image:
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW:
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW:
      case ShaderReflection::Tlas:
        ret.first++;
        break;
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        break;
      }
    }
  return ret;
  }


VPushDescriptor::DescPool::DescPool(VDevice &dev) {
  impl = dev.descPool.allocPool();
  }

VPushDescriptor::ResPool::ResPool(VDevice &dev, uint32_t size) {
  auto mem = dev.resHeap.alloc(size);
  dPtr  = mem.ptr;
  alloc = 0;
  }

VPushDescriptor::VPushDescriptor(VDevice &dev)
  :dev(dev) {
  }

VPushDescriptor::~VPushDescriptor() {
  reset();
  }

void VPushDescriptor::reset() {
  descPool.reserve(descPool.size());
  for(auto& i:descPool)
    dev.descPool.freePool(i.impl);
  descPool.clear();

  resPool.reserve(resPool.size());
  for(auto& i:resPool) {
    dev.resHeap.free(i.dPtr, RES_ALLOC_SZ);
    }
  resPool.clear();
  memHeap.clear();

  lastResHeap = nullptr;
  lastSmpHeap = nullptr;
  }

void VPushDescriptor::onNextCmdChunk() {
  lastResHeap = nullptr;
  lastSmpHeap = nullptr;
  }

VkDescriptorSet VPushDescriptor::allocSet(const VkDescriptorSetLayout dLayout) {
  if(descPool.empty())
    descPool.emplace_back(DescPool(dev));

  for(int i=0; i<2; ++i) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descPool.back().impl;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &dLayout;

    VkDescriptorSet desc = VK_NULL_HANDLE;
    VkResult        ret  = vkAllocateDescriptorSets(dev.device.impl, &allocInfo, &desc);
    if(ret==VK_ERROR_FRAGMENTED_POOL || ret==VK_ERROR_OUT_OF_POOL_MEMORY) {
      descPool.emplace_back(DescPool(dev));
      continue;
      }
    vkAssert(ret);
    return desc;
    }
  vkAssert(VK_ERROR_OUT_OF_HOST_MEMORY);
  return VK_NULL_HANDLE;
  }

VkDescriptorSet VPushDescriptor::allocSet(const LayoutDesc& lay) {
  auto lt = dev.setLayouts.findLayout(lay);
  return allocSet(lt);
  }

uint32_t VPushDescriptor::allocHeap(VkCommandBuffer cmd, const uint32_t sz, const uint32_t step) {
  if(resPool.empty()) {
    resPool.emplace_back(dev, step);
    }

  if((step-resPool.back().alloc) < sz) {
    resPool.emplace_back(dev, step);
    }

  // FIXME: pool-allocation might be outdated
  auto& px = resPool.back();
  const uint32_t ptr = px.dPtr + px.alloc;
  px.alloc += sz;
  return ptr;
  }

void VPushDescriptor::pushHeap(VkCommandBuffer cmd, uint32_t* indices, const PushBlock& pb, const LayoutDesc& lay, const Bindings& binding) {
  const auto sz  = numResources(lay);
  auto       ptr = allocHeap(cmd, sz.first, RES_ALLOC_SZ);

  const auto resSize = dev.props.resourceDescriptorSize;

  auto mem = sz.first>0  ? dev.resHeap.currentMemory()  : nullptr;

  auto res = mem==nullptr ? nullptr : mem->hptr;
  res += ptr*resSize;
  for(size_t i=0; i<MaxBindings; ++i) {
    if(((1u << i) & lay.active)==0)
      continue;
    if(((1u << i) & lay.array)!=0) {
      const auto data = reinterpret_cast<VDescriptorHeapArray*>(binding.data[i]);
      if(lay.bindings[i]==ShaderReflection::Texture) {
        indices[0] = (data->handleR() & 0xFFFFF) | (data->handleS() << 20);
        }
      else if(lay.bindings[i]==ShaderReflection::Sampler) {
        indices[0] = data->handleS();
        }
      else {
        indices[0] = data->handleR();
        }
      ++indices;
      continue;
      }

    VPushDescriptor::write(dev, res, lay.bindings[i], binding.data[i], binding.offset[i], binding.map[i]);

    if(lay.bindings[i]!=ShaderReflection::Sampler && lay.bindings[i]!=ShaderReflection::Texture) {
      indices[0] = ptr; ++indices;
      res += resSize;
      ptr += 1;
      }
    if(lay.bindings[i]==ShaderReflection::Texture) {
      const auto smpId = dev.samplers.getH(binding.smp[i]);
      indices[0] = (ptr & 0xFFFFF) | (smpId << 20);
      res += resSize;
      ptr += 1;
      ++indices;
      }
    if(lay.bindings[i]==ShaderReflection::Sampler) {
      indices[0] = dev.samplers.getH(binding.smp[i]); ++indices;
      }
    }

  auto smp = sz.second>0 ? dev.samplers.currentMemory() : nullptr;
  bindHeap(cmd, mem, smp);
  }

void VPushDescriptor::bindHeap(VkCommandBuffer cmd, const std::shared_ptr<VBuffer>& res, const std::shared_ptr<VBuffer>& smp) {
  auto vkCmdBindResourceHeapEXT = dev.vkCmdBindResourceHeapEXT;

  if(res!=nullptr && lastResHeap!=res.get()) {
    lastResHeap = res.get();
    memHeap.push_back(res);

    auto& resources = *lastResHeap;
    VkBindHeapInfoEXT info = {VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    info.heapRange.address   = resources.toDeviceAddress(dev);
    info.heapRange.size      = resources.size();
    info.reservedRangeOffset = 0;
    info.reservedRangeSize   = dev.props.resourceHeapReserve;
    vkCmdBindResourceHeapEXT(cmd, &info);
    }

  if(smp!=nullptr && lastSmpHeap!=smp.get()) {
    lastSmpHeap = smp.get();
    memHeap.push_back(smp);
    dev.samplers.bindHeap(cmd, *lastSmpHeap);
    }
  }

VkDescriptorSet VPushDescriptor::push(const PushBlock& pb, const LayoutDesc& lay, const Bindings& binding) {
  auto set = allocSet(lay);

  WriteInfo              winfo[MaxBindings] = {};
  VkWriteDescriptorSet   wr   [MaxBindings] = {};
  uint32_t               cntWr = 0;

  for(size_t i=0; i<MaxBindings; ++i) {
    auto  cls = lay.bindings[i];
    auto& wx  = wr[cntWr];
    VPushDescriptor::write(dev, wx, winfo[cntWr], uint32_t(i), cls,
                           binding.data[i], binding.offset[i], binding.map[i], binding.smp[i]);
    wx.dstSet = set;
    if(wx.descriptorCount>0)
      ++cntWr;
    }

  vkUpdateDescriptorSets(dev.device.impl, cntWr, wr, 0, nullptr);
  return set;
  }

void VPushDescriptor::write(VDevice& dev, VkWriteDescriptorSet& wx, WriteInfo& infoW, uint32_t dstBinding,
                            ShaderReflection::Class cls, AbstractGraphicsApi::NoCopy* data, uint32_t offset,
                            const ComponentMapping& mapping, const Sampler& smp) {
  switch(cls) {
    case ShaderReflection::Ubo:
    case ShaderReflection::SsboR:
    case ShaderReflection::SsboRW: {
      auto* buf = reinterpret_cast<VBuffer*>(data);

      VkDescriptorBufferInfo& info = infoW.buffer;
      info.buffer = buf!=nullptr ? buf->impl : VK_NULL_HANDLE;
      info.offset = offset;
      //TODO: head only ssbo
      info.range  = VK_WHOLE_SIZE; //(buf!=nullptr && slot.varByteSize==0) ? slot.byteSize : VK_WHOLE_SIZE;

      if(!dev.props.hasRobustness2 && buf==nullptr) {
        //NOTE1: use of null-handle is not allowed, unless VK_EXT_robustness2
        //NOTE2: sizeof 1 is rouned up in shader; and sizeof 0 is illegal but harmless(hopefully)
        info.buffer = dev.dummySsbo().impl;
        info.offset = 0;
        info.range  = 0;
        }

      wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wx.dstSet          = VK_NULL_HANDLE;
      wx.dstBinding      = dstBinding;
      wx.dstArrayElement = 0;
      wx.descriptorType  = nativeFormat(cls);
      wx.descriptorCount = 1;
      wx.pBufferInfo     = &info;
      break;
      }
    case ShaderReflection::Texture:
    case ShaderReflection::Image:
    case ShaderReflection::ImgR:
    case ShaderReflection::ImgRW: {
      auto*    tex       = reinterpret_cast<VTexture*>(data);
      uint32_t mipLevel  = offset;
      bool     is3DImage = tex->is3D; // TODO: cast 3d to 2d, based on dest descriptor

      if((cls==ShaderReflection::ImgR || cls==ShaderReflection::ImgRW) && mipLevel==uint32_t(-1))
        mipLevel = 0;

      VkDescriptorImageInfo& info = infoW.image;
      if(cls==ShaderReflection::Texture) {
        auto sx = smp;
        if(!tex->isFilterable) {
          sx.setFiltration(Filter::Nearest);
          sx.anisotropic = false;
          }
        info.sampler   = dev.samplers.get(sx);
        info.imageView = tex->view(mapping, mipLevel, is3DImage);
        } else {
        info.imageView = tex->view(mapping, mipLevel, is3DImage);
        }
      info.imageLayout = toWriteLayout(*tex);

      wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wx.dstSet          = VK_NULL_HANDLE;
      wx.dstBinding      = dstBinding;
      wx.dstArrayElement = 0;
      wx.descriptorType  = nativeFormat(cls);
      wx.descriptorCount = 1;
      wx.pImageInfo      = &info;
      break;
      }
    case ShaderReflection::Sampler: {
      VkDescriptorImageInfo& info = infoW.image;
      info.sampler = dev.samplers.get(smp);

      wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wx.dstSet          = VK_NULL_HANDLE;
      wx.dstBinding      = dstBinding;
      wx.dstArrayElement = 0;
      wx.descriptorType  = nativeFormat(cls);
      wx.descriptorCount = 1;
      wx.pImageInfo      = &info;
      break;
      }
    case ShaderReflection::Tlas: {
      auto* tlas = reinterpret_cast<VAccelerationStructure*>(data);

      auto& info = infoW.tlas;
      info.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
      info.accelerationStructureCount = 1;
      info.pAccelerationStructures    = &tlas->impl;

      wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wx.pNext           = &info;
      wx.dstSet          = VK_NULL_HANDLE;
      wx.dstBinding      = dstBinding;
      wx.dstArrayElement = 0;
      wx.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
      wx.descriptorCount = 1;
      break;
      }
    case ShaderReflection::Push:
    case ShaderReflection::Count:
      break;
    }
  }

void VPushDescriptor::write(VDevice& dev, void* resPtr, void* smpPtr, ShaderReflection::Class cls,
                            AbstractGraphicsApi::NoCopy* data, uint32_t offset, const ComponentMapping& mapping, const Sampler& smp) {
  auto vkWriteResourceDescriptorsEXT = dev.vkWriteResourceDescriptorsEXT;

  VkResourceDescriptorInfoEXT res = {VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT};
  switch(cls) {
    case ShaderReflection::Ubo:
      res.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      break;
    case ShaderReflection::Texture:
    case ShaderReflection::Image:
      res.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      break;
    case ShaderReflection::Sampler:
      res.type = VK_DESCRIPTOR_TYPE_SAMPLER;
      break;
    case ShaderReflection::SsboR:
    case ShaderReflection::SsboRW:
      res.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      break;
    case ShaderReflection::ImgR:
    case ShaderReflection::ImgRW:
      res.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      break;
    case ShaderReflection::Tlas:
      res.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
      break;
    case ShaderReflection::Push:
    case ShaderReflection::Count:
      break;
    }

  switch(cls) {
    case ShaderReflection::Ubo:
    case ShaderReflection::SsboR:
    case ShaderReflection::SsboRW:{
      auto* buf = reinterpret_cast<VBuffer*>(data);

      VkDeviceAddressRangeEXT info = {};
      info.address = buf!=nullptr ? buf->toDeviceAddress(dev) + offset : 0;
      info.size    = buf!=nullptr ? buf->size() - offset : 0;

      //NOTE1: assume VK_EXT_robustness2, for sake of null descriptor
      res.data.pAddressRange = buf!=nullptr ? &info : nullptr;

      VkHostAddressRangeEXT dest = {resPtr, dev.props.resourceDescriptorSize};
      vkAssert(vkWriteResourceDescriptorsEXT(dev.device.impl, 1, &res, &dest));
      break;
      }
    case ShaderReflection::Texture:
    case ShaderReflection::Image:
    case ShaderReflection::ImgR:
    case ShaderReflection::ImgRW:{
      if(data==nullptr)
        return;
      auto*    tex       = reinterpret_cast<VTexture*>(data);
      uint32_t mipLevel  = offset;
      bool     is3DImage = tex->is3D; // TODO: cast 3d to 2d, based on dest descriptor

      if((cls==ShaderReflection::ImgR || cls==ShaderReflection::ImgRW) && mipLevel==uint32_t(-1))
        mipLevel = 0;

      VkImageViewCreateInfo view = tex->createInfo(&mapping, mipLevel, is3DImage);

      VkImageDescriptorInfoEXT info = {VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT};
      info.pView  = &view;
      info.layout = toWriteLayout(*tex);

      res.data.pImage = &info;

      VkHostAddressRangeEXT dest = {resPtr, dev.props.resourceDescriptorSize};
      vkAssert(vkWriteResourceDescriptorsEXT(dev.device.impl, 1, &res, &dest));
      break;
      }
    case ShaderReflection::Tlas: {
      auto* tlas = reinterpret_cast<VAccelerationStructure*>(data);

      VkDeviceAddressRangeEXT info = {};
      info.address = tlas!=nullptr ? tlas->toDeviceAddress(dev) : 0;
      info.size    = 0; //buf!=nullptr  ? buf->size() : 0;

      res.data.pAddressRange = &info;

      VkHostAddressRangeEXT dest = {resPtr, dev.props.resourceDescriptorSize};
      vkAssert(vkWriteResourceDescriptorsEXT(dev.device.impl, 1, &res, &dest));
      break;
      }
    case ShaderReflection::Sampler:
    case ShaderReflection::Push:
    case ShaderReflection::Count:
      break;
    }
  }

#endif

