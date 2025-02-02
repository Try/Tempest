#if defined(TEMPEST_BUILD_VULKAN)

#include "vbindlesscache.h"

#include "gapi/vulkan/vdescriptorarray.h"
#include "gapi/vulkan/vdevice.h"
#include "gapi/vulkan/vpipelinelay.h"
#include "gapi/vulkan/vpipeline.h"
#include "gapi/vulkan/vtexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

static VkImageLayout toWriteLayout(VTexture& tex) {
  if(nativeIsDepthFormat(tex.format))
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  if(tex.isStorageImage)
    return VK_IMAGE_LAYOUT_GENERAL;
  return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

bool VBindlessCache::Bindings::operator ==(const Bindings &other) const {
  for(size_t i=0; i<MaxBindings; ++i) {
    if(data[i]!=other.data[i])
      return false;
    if(smp[i]!=other.smp[i])
      return false;
    if(offset[i]!=other.offset[i])
      return false;
    }
  return array==other.array;
  }

bool VBindlessCache::Bindings::operator !=(const Bindings &other) const {
  return !(*this==other);
  }

bool VBindlessCache::Bindings::contains(const AbstractGraphicsApi::NoCopy* res) const {
  for(size_t i=0; i<MaxBindings; ++i)
    if(data[i]==res)
      return true;
  return false;
  }

bool VBindlessCache::LayoutDesc::operator ==(const LayoutDesc &other) const {
  if(std::memcmp(bindings, other.bindings, sizeof(bindings))!=0)
    return false;
  if(std::memcmp(count, other.count, sizeof(count))!=0)
    return false;
  if(runtime!=other.runtime || array!=other.array)
    return false;
  return true;
  }

bool VBindlessCache::LayoutDesc::operator != (const LayoutDesc& other) const {
  return !(*this==other);
  }

bool VBindlessCache::LayoutDesc::isUpdateAfterBind() const {
  return runtime!=0 || array!=0;
  }

VBindlessCache::VBindlessCache(VDevice& dev)
  : dev(dev) {
  }

VBindlessCache::~VBindlessCache() {
  for(auto& i:descriptors) {
    (void)i;
    // should be deleted already
    // vkDestroyDescriptorPool(dev.device.impl, i.pool, nullptr);
    }

  for(auto& i:pLayouts)
    vkDestroyPipelineLayout(dev.device.impl, i.pLay, nullptr);
  for(auto& i:layouts)
    vkDestroyDescriptorSetLayout(dev.device.impl, i.lay, nullptr);
  }

void VBindlessCache::notifyDestroy(const AbstractGraphicsApi::NoCopy *res) {
  std::lock_guard<std::mutex> guard(syncDesc);

  for(size_t i=0; i<descriptors.size();) {
    auto& d = descriptors[i];
    if(!d.bindings.contains(res)) {
      ++i;
      continue;
      }
    vkDestroyDescriptorPool(dev.device.impl, d.pool, nullptr);
    d = std::move(descriptors.back());
    descriptors.pop_back();
    }
  }

VBindlessCache::Inst VBindlessCache::inst(const VCompPipeline& pso, const Bindings &binding) {
  auto lx   = toLayout(pso.layout(), binding);
  auto lt   = findLayout(lx);
  auto px   = findPsoLayout(pso.layout(), lt.lay);

  Inst ret;
  ret.lay  = lt.lay;
  ret.pLay = px.pLay;

  std::lock_guard<std::mutex> guard(syncDesc);
  for(auto& i:descriptors) {
    if(i.bindings!=binding)
      continue;
    ret.set = i.set;
    return ret;
    }

  auto& desc = descriptors.emplace_back();
  try {
    desc.bindings = binding;
    desc.pool     = allocPool(lx);
    desc.set      = allocDescSet(desc.pool, lt.lay);
    initDescriptorSet(desc.set, binding, lx);
    }
  catch(...) {
    if(desc.pool!=VK_NULL_HANDLE)
      vkDestroyDescriptorPool(dev.device.impl, desc.pool, nullptr);
    descriptors.pop_back();
    throw;
    }

  ret.set  = desc.set;
  ret.lay  = lt.lay;
  ret.pLay = px.pLay;
  return ret;
  }

VBindlessCache::LayoutDesc VBindlessCache::toLayout(const VPipelineLay& pLay, const Bindings &binding) {
  auto& lay  = pLay.lay;

  LayoutDesc lx;
  std::fill(std::begin(lx.bindings), std::end(lx.bindings), ShaderReflection::Count);

  for(size_t i=0; i<lay.size(); ++i) {
    auto& e = lay[i];
    auto* a = e.runtimeSized ? reinterpret_cast<const VDescriptorArray2*>(binding.data[e.layout]) : nullptr;

    if(e.stage==ShaderReflection::Stage(0))
      continue;

    lx.bindings[e.layout] = e.cls;
    lx.count   [e.layout] = e.runtimeSized ? uint32_t(a->size()) : e.arraySize;
    lx.stage   [e.layout] = e.stage;
    if(e.runtimeSized)
      lx.runtime |= (1u << e.layout);
    if(e.arraySize>1)
      lx.array   |= (1u << e.layout);
    }

  return lx;
  }

VBindlessCache::Layout VBindlessCache::findLayout(const LayoutDesc &l) {
  std::lock_guard<std::mutex> guard(syncLay);
  for(auto& i:layouts) {
    if(i.desc!=l)
      continue;
    return i;
    }

  Layout& ret = layouts.emplace_back();
  ret.desc = l;

  VkDescriptorSetLayoutBinding bind[MaxBindings] = {};
  VkDescriptorBindingFlags     flg [MaxBindings] = {};
  uint32_t                     count             = 0;
  for(size_t i=0; i<MaxBindings; ++i) {
    if(l.bindings[i]==ShaderReflection::Count)
      continue;

    auto& b = bind[count];
    if((l.runtime & (1u << i))!=0)
      flg[count] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    b.binding         = uint32_t(i);
    b.descriptorCount = l.count[i];
    b.descriptorCount = std::max<uint32_t>(1, b.descriptorCount); // WA for VUID-VkGraphicsPipelineCreateInfo-layout-07988
    b.descriptorType  = nativeFormat(l.bindings[i]);
    b.stageFlags      = nativeFormat(l.stage[i]);
    ++count;
    }

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags = {};
  bindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlags.bindingCount  = count;
  bindingFlags.pBindingFlags = flg;

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext        = nullptr;
  info.flags        = 0;
  info.bindingCount = count;
  info.pBindings    = bind;

  if(l.isUpdateAfterBind()) {
    info.pNext  = &bindingFlags;
    info.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    }

  try {
    vkAssert(vkCreateDescriptorSetLayout(dev.device.impl, &info, nullptr, &ret.lay));
    }
  catch(...) {
    layouts.pop_back();
    throw;
    }
  return ret;
  }

VBindlessCache::PLayout VBindlessCache::findPsoLayout(const VPipelineLay& pLay, VkDescriptorSetLayout lay) {
  std::lock_guard<std::mutex> guard(syncPLay);

  const auto stageFlags = nativeFormat(pLay.pb.stage);
  for(auto& i:pLayouts)
    if(i.lay==lay && i.pushStage==stageFlags && i.pushSize==pLay.pb.size)
      return i;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.pSetLayouts            = &lay;
  pipelineLayoutInfo.setLayoutCount         = 1;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  VkPushConstantRange push = {};
  if(pLay.pb.size>0) {
    push.stageFlags = nativeFormat(pLay.pb.stage);
    push.offset     = 0;
    push.size       = uint32_t(pLay.pb.size);

    pipelineLayoutInfo.pPushConstantRanges    = &push;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

  auto& ret = pLayouts.emplace_back();
  ret.lay       = lay;
  ret.pushStage = stageFlags;
  ret.pushSize  = uint32_t(pLay.pb.size);
  try {
    vkAssert(vkCreatePipelineLayout(dev.device.impl, &pipelineLayoutInfo, nullptr, &ret.pLay));
    }
  catch(...) {
    pLayouts.pop_back();
    throw;
    }
  return ret;
  }

void VBindlessCache::addPoolSize(VkDescriptorPoolSize *p, size_t &sz, uint32_t cnt, VkDescriptorType elt) {
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

VkDescriptorPool VBindlessCache::allocPool(const LayoutDesc &l) {
  VkDescriptorPoolSize poolSize[int(ShaderReflection::Class::Count)] = {};
  size_t               pSize   = 0;

  for(size_t i=0; i<MaxBindings; ++i) {
    if(l.stage[i]==Tempest::Detail::ShaderReflection::None)
      continue;
    auto     cls = l.bindings[i];
    uint32_t cnt = l.count[i];
    if((l.runtime & (1u << i))!=0)
      cnt = std::max<uint32_t>(1, l.count[i]);
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

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = 1;
  poolInfo.flags         = 0;
  poolInfo.poolSizeCount = uint32_t(pSize);
  poolInfo.pPoolSizes    = poolSize;

  if(l.isUpdateAfterBind())
    poolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

  VkDevice dev = this->dev.device.impl;
  VkDescriptorPool ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorPool(dev,&poolInfo,nullptr,&ret));
  return ret;
  }

VkDescriptorSet VBindlessCache::allocDescSet(VkDescriptorPool pool, VkDescriptorSetLayout lay) {
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &lay;

  VkDevice        dev  = this->dev.device.impl;
  VkDescriptorSet desc = VK_NULL_HANDLE;
  VkResult        ret  = vkAllocateDescriptorSets(dev,&allocInfo,&desc);
  if(ret==VK_ERROR_FRAGMENTED_POOL)
    return VK_NULL_HANDLE;
  if(ret!=VK_SUCCESS)
    return VK_NULL_HANDLE;
  return desc;
  }

void VBindlessCache::initDescriptorSet(VkDescriptorSet dset, const Bindings &binding, const LayoutDesc& l) {
  VkCopyDescriptorSet    cpy[MaxBindings] = {};
  uint32_t               cntCpy = 0;

  VkWriteDescriptorSet   wr[MaxBindings] = {};
  uint32_t               cntWr = 0;

  VkDescriptorBufferInfo bufferInfo[MaxBindings] = {};
  VkDescriptorImageInfo  imageInfo [MaxBindings] = {};

  for(size_t i=0; i<MaxBindings; ++i) {
    VkCopyDescriptorSet& cx = cpy[cntCpy];

    if((binding.array & (1u << i))!=0) {
      // assert((l.runtime & (1u << i))!=0);
      auto arr = reinterpret_cast<const VDescriptorArray2*>(binding.data[i]);
      cx.sType           = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
      cx.pNext           = nullptr;
      cx.srcSet          = arr->set();
      cx.srcBinding      = 0;
      cx.srcArrayElement = 0;
      cx.dstSet          = dset;
      cx.dstBinding      = uint32_t(i);
      cx.dstArrayElement = 0;
      cx.descriptorCount = uint32_t(arr->size());
      if(cx.descriptorCount>0)
        ++cntCpy;
      continue;
      }

    VkWriteDescriptorSet& wx = wr[cntWr];
    switch(l.bindings[i]) {
      case ShaderReflection::Ubo:
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW: {
        auto* buf = reinterpret_cast<VBuffer*>(binding.data[i]);

        VkDescriptorBufferInfo& info = bufferInfo[i];
        info.buffer = buf!=nullptr ? buf->impl : VK_NULL_HANDLE;
        info.offset = binding.offset[i];
        info.range  = VK_WHOLE_SIZE; //(buf!=nullptr && slot.varByteSize==0) ? slot.byteSize : VK_WHOLE_SIZE;

        if(!dev.props.hasRobustness2 && buf==nullptr) {
          //NOTE1: use of null-handle is not allowed, unless VK_EXT_robustness2
          //NOTE2: sizeof 1 is rouned up in shader; and sizeof 0 is illegal but harmless(hepefully)
          info.buffer = dev.dummySsbo().impl;
          info.offset = 0;
          info.range  = 0;
          }

        wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wx.dstSet          = dset;
        wx.dstBinding      = uint32_t(i);
        wx.dstArrayElement = 0;
        wx.descriptorType  = nativeFormat(l.bindings[i]);
        wx.descriptorCount = 1;
        wx.pBufferInfo     = &info;
        break;
        }
      case ShaderReflection::Texture:
      case ShaderReflection::Image:
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW: {
        auto*    tex       = reinterpret_cast<VTexture*>(binding.data[i]);
        uint32_t mipLevel  = binding.offset[i];
        bool     is3DImage = false; // TODO

        VkDescriptorImageInfo& info = imageInfo[i];
        if(l.bindings[i]==ShaderReflection::Texture) {
          auto sx = binding.smp[i];
          if(!tex->isFilterable) {
            sx.setFiltration(Filter::Nearest);
            sx.anisotropic = false;
            }
          info.sampler   = dev.allocator.updateSampler(sx);
          info.imageView = tex->view(sx.mapping, mipLevel, is3DImage);
          } else {
          info.imageView = tex->view(ComponentMapping(), mipLevel, is3DImage);
          }
        info.imageLayout = toWriteLayout(*tex);

        wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wx.dstSet          = dset;
        wx.dstBinding      = uint32_t(i);
        wx.dstArrayElement = 0;
        wx.descriptorType  = nativeFormat(l.bindings[i]);
        wx.descriptorCount = 1;
        wx.pImageInfo      = &info;
        break;
        }
      case ShaderReflection::Sampler: {
        VkDescriptorImageInfo& info = imageInfo[i];
        info.sampler = dev.allocator.updateSampler(binding.smp[i]);

        wx.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wx.dstSet          = dset;
        wx.dstBinding      = uint32_t(i);
        wx.dstArrayElement = 0;
        wx.descriptorType  = nativeFormat(l.bindings[i]);
        wx.descriptorCount = 1;
        wx.pImageInfo      = &info;
        break;
        }
      case ShaderReflection::Tlas:
        break;
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        break;
      }
    if(wx.descriptorCount>0)
      ++cntWr;
    }

  vkUpdateDescriptorSets(dev.device.impl, cntWr, wr, cntCpy, cpy);
  }

#endif
