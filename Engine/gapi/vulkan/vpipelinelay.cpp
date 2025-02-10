#if defined(TEMPEST_BUILD_VULKAN)
#include "vpipelinelay.h"

#include <Tempest/PipelineLayout>
#include <cstring>

#include "vdevice.h"
#include "vpipeline.h"
#include "gapi/shaderreflection.h"
#include "utility/smallarray.h"

using namespace Tempest;
using namespace Tempest::Detail;

VPipelineLay::VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh)
  : VPipelineLay(dev,&sh,1) {
  }

VPipelineLay::VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt)
  : dev(dev) {
  setupLayout(pb, layout, sync, sh, cnt);

  ShaderReflection::merge(lay, pb, sh, cnt);
  adjustSsboBindings();

  // TODO: avoid creating dummy impl for bindless
  std::vector<uint32_t> runtimeArrays;
  if(runtimeSized)
    runtimeArrays.resize(lay.size());
  impl = createDescLayout(runtimeArrays);
  }

VPipelineLay::~VPipelineLay() {
  for(auto& i:pool)
    vkDestroyDescriptorPool(dev.device.impl,i.impl,nullptr);
  vkDestroyDescriptorSetLayout(dev.device.impl,impl,nullptr);

  for(auto& i:dedicatedLay) {
    vkDestroyPipelineLayout(dev.device.impl,i.pLay,nullptr);
    vkDestroyDescriptorSetLayout(dev.device.impl,i.dLay,nullptr);
    }
  }

size_t VPipelineLay::descriptorsCount() {
  return lay.size();
  }

size_t VPipelineLay::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return ShaderReflection::sizeofBuffer(lay[layoutBind], arraylen);
  }

bool VPipelineLay::isUpdateAfterBind() const {
  return runtimeSized;
  }

VPipelineLay::DedicatedLay VPipelineLay::create(const std::vector<uint32_t>& runtimeArrays) {
  std::lock_guard<Detail::SpinLock> guard(syncLay);

  for(auto& i:dedicatedLay) {
    if(i.runtimeArrays==runtimeArrays)
      return DedicatedLay(i);
    }

  DLay ret;
  ret.runtimeArrays = runtimeArrays;

  ret.dLay = createDescLayout(runtimeArrays);
  if(ret.dLay==VK_NULL_HANDLE) {
    throw std::bad_alloc();
    }

  ret.pLay = VPipeline::initLayout(dev, *this, ret.dLay);
  if(ret.pLay==VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(dev.device.impl,ret.dLay,nullptr);
    throw std::bad_alloc();
    }

  try {
    dedicatedLay.push_back(ret);
    }
  catch (...) {
    vkDestroyPipelineLayout(dev.device.impl,ret.pLay,nullptr);
    vkDestroyDescriptorSetLayout(dev.device.impl,ret.dLay,nullptr);
    }

  return ret;
  }

VkDescriptorSetLayout VPipelineLay::createDescLayout(const std::vector<uint32_t>& runtimeArrays) const {
  SmallArray<VkDescriptorSetLayoutBinding,32> bind(lay.size());
  SmallArray<VkDescriptorBindingFlags,32>     flg (lay.size());

  uint32_t count = 0;
  for(size_t i=0; i<lay.size(); ++i) {
    auto& b = bind[count];
    auto& e = lay[i];

    if(e.stage==ShaderReflection::Stage(0))
      continue;

    if(e.runtimeSized)
      flg[count] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT; else
      flg[count] = 0;

    b.binding         = e.layout;
    b.descriptorCount = e.runtimeSized ? runtimeArrays[i] : e.arraySize;
    b.descriptorCount = std::max<uint32_t>(1, b.descriptorCount); // WA for VUID-VkGraphicsPipelineCreateInfo-layout-07988
    b.descriptorType  = nativeFormat(e.cls);
    b.stageFlags      = nativeFormat(e.stage);
    ++count;
    }

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags = {};
  bindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlags.bindingCount  = count;
  bindingFlags.pBindingFlags = flg.get();

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext        = nullptr;
  info.flags        = 0;
  info.bindingCount = count;
  info.pBindings    = bind.get();

  if(runtimeSized) {
    info.pNext  = &bindingFlags;
    info.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    }

  VkDescriptorSetLayout ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorSetLayout(dev.device.impl,&info,nullptr,&ret));
  return ret;
  }

VkDescriptorSetLayout VPipelineLay::createMsHelper() const {
  VkDescriptorSetLayoutBinding bind[1] = {};
  bind[0].binding         = 0;
  bind[0].descriptorCount = 1;
  bind[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bind[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext        = nullptr;
  info.flags        = 0;
  info.bindingCount = 1; // one for vertex
  info.pBindings    = bind;

  VkDescriptorSetLayout ret = VK_NULL_HANDLE;
  vkAssert(vkCreateDescriptorSetLayout(dev.device.impl,&info,nullptr,&ret));
  return ret;
  }

void VPipelineLay::adjustSsboBindings() {
  for(auto& i:lay) {
    if(i.byteSize==0) {
      i.byteSize = VK_WHOLE_SIZE;
      }
    if(i.runtimeSized) {
      runtimeSized = true;
      }
    }
  }

void VPipelineLay::setupLayout(ShaderReflection::PushBlock& pb, LayoutDesc& lx, SyncDesc& sync,
                               const std::vector<ShaderReflection::Binding>* sh[], size_t cnt) {
  std::fill(std::begin(lx.bindings), std::end(lx.bindings), ShaderReflection::Count);

  for(size_t i=0; i<cnt; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto& bind = *sh[i];
    for(auto& e:bind) {
      if(e.cls==ShaderReflection::Push) {
        pb.stage = ShaderReflection::Stage(pb.stage | e.stage);
        pb.size  = std::max(pb.size, e.byteSize);
        pb.size  = std::max(pb.size, e.mslSize);
        continue;
        }
      const uint32_t id = (1u << e.layout);

      lx.bindings[e.layout] = e.cls;
      lx.count   [e.layout] = std::max(lx.count[e.layout] , e.arraySize);
      lx.stage   [e.layout] = ShaderReflection::Stage(e.stage | lx.stage[e.layout]);

      if(e.runtimeSized)
        lx.runtime |= id;
      if(e.runtimeSized || e.arraySize>1)
        lx.array   |= id;
      lx.active |= id;

      sync.read |= id;
      if(e.cls==ShaderReflection::ImgRW || e.cls==ShaderReflection::SsboRW)
        sync.write |= id;

      lx.bufferSz[e.layout] = std::max<uint32_t>(lx.bufferSz[e.layout], e.byteSize);
      lx.bufferEl[e.layout] = std::max<uint32_t>(lx.bufferEl[e.layout], e.varByteSize);
      }
    }
  }

#endif

