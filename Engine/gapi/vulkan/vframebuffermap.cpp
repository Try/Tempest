#if defined(TEMPEST_BUILD_VULKAN)

#include "vframebuffermap.h"

#include "vdevice.h"
#include "vtexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

VFramebufferMap::RenderPassDesc::RenderPassDesc(const VkRenderingInfo& info, const VkPipelineRenderingCreateInfoKHR& fbo) {
  for(uint8_t i=0; i<info.colorAttachmentCount; ++i) {
    attachmentFormats[i] = fbo.pColorAttachmentFormats[i];
    loadOp[i]            = info.pColorAttachments[i].loadOp;
    storeOp[i]           = info.pColorAttachments[i].storeOp;
    }
  if(info.pDepthAttachment!=nullptr) {
    const auto i = info.colorAttachmentCount;
    attachmentFormats[i] = fbo.depthAttachmentFormat;
    loadOp[i]            = info.pDepthAttachment->loadOp;
    storeOp[i]           = info.pDepthAttachment->storeOp;
    }
  numAttachments = info.colorAttachmentCount +(info.pDepthAttachment!=nullptr ? 1 : 0);
  }

bool VFramebufferMap::RenderPassDesc::operator ==(const RenderPassDesc& other) const {
  if(numAttachments!=other.numAttachments)
    return false;
  return std::memcmp(attachmentFormats, other.attachmentFormats, numAttachments*sizeof(attachmentFormats[0]))==0 &&
         std::memcmp(loadOp,  other.loadOp,  numAttachments*sizeof(loadOp[0]))==0 &&
         std::memcmp(storeOp, other.storeOp, numAttachments*sizeof(storeOp[0]))==0;
  }

bool VFramebufferMap::RenderPass::isCompatible(const RenderPass& other) const {
  if(desc.numAttachments!=other.desc.numAttachments)
    return false;

  for(uint32_t i=0; i<desc.numAttachments; ++i) {
    if(desc.attachmentFormats[i]!=other.desc.attachmentFormats[i])
      return false;
    }

  return true;
  }

bool VFramebufferMap::RenderPass::isSame(const RenderPassDesc& other) const {
  return desc==other;
  }

bool VFramebufferMap::Fbo::isSame(const VkImageView* attach, size_t attCount, const RenderPassDesc& desc) const {
  if(descSize!=attCount)
    return false;
  return std::memcmp(view, attach, attCount*sizeof(VkImageView))==0 && pass->desc==desc;
  }

bool VFramebufferMap::Fbo::hasImg(VkImageView v) const {
  for(size_t i=0; i<descSize; ++i)
    if(view[i]==v)
      return true;
  return false;
  }

VFramebufferMap::VFramebufferMap(VDevice& dev) : dev(dev) {
  }

VFramebufferMap::~VFramebufferMap() {
  auto device = dev.device.impl;
  if(device==VK_NULL_HANDLE)
    return;

  for(auto& i:val) {
    auto& v = *i;
    if(v.fbo!=VK_NULL_HANDLE)
      vkDestroyFramebuffer(device,v.fbo,nullptr);
    }
  for(auto& i:rp) {
    auto& v = *i;
    if(v.pass!=VK_NULL_HANDLE)
      vkDestroyRenderPass(device,v.pass,nullptr);
    }
  }

void VFramebufferMap::notifyDestroy(VkImageView img) {
  auto device = dev.device.impl;

  std::lock_guard<std::mutex> guard(syncFbo);
  for(size_t i=0; i<val.size();) {
    auto& v = *val[i];
    if(!v.hasImg(img)) {
      ++i;
      continue;
      }
    vkDestroyFramebuffer(device,v.fbo,nullptr);
    val[i] = std::move(val.back());
    val.pop_back();
    }
  }

std::shared_ptr<VFramebufferMap::Fbo> VFramebufferMap::find(const VkRenderingInfo* info, const VkPipelineRenderingCreateInfoKHR* fbo) {
  VkImageView attach[MaxFramebufferAttachments] = {};
  for(uint8_t i=0; i<info->colorAttachmentCount; ++i) {
    attach[i] = info->pColorAttachments[i].imageView;
    }
  if(info->pDepthAttachment!=nullptr) {
    const uint32_t i = info->colorAttachmentCount;
    attach[i] = info->pDepthAttachment->imageView;
    }
  const uint32_t attCount = info->colorAttachmentCount + (info->pDepthAttachment!=nullptr ? 1 : 0);

  RenderPassDesc desc(*info, *fbo);
  std::lock_guard<std::mutex> guard(syncFbo);
  for(auto& i:val)
    if(i->isSame(attach, attCount, desc))
      return i;
  val.push_back(std::make_shared<Fbo>());
  try {
    *val.back() = mkFbo(info, fbo, attach, attCount);
    }
  catch(...) {
    val.pop_back();
    throw;
    }
  return val.back();
  }

VFramebufferMap::Fbo VFramebufferMap::mkFbo(const VkRenderingInfo* info, const VkPipelineRenderingCreateInfoKHR* fbo, const VkImageView* attach, size_t attCount) {
  Fbo ret = {};
  ret.pass = findRenderpass(info, fbo);
  ret.fbo  = mkFramebuffer(attach, attCount, info->renderArea.extent, ret.pass->pass);

  std::memcpy(ret.view, attach, attCount*sizeof(VkImageView));
  ret.descSize = uint8_t(attCount);
  return ret;
  }

std::shared_ptr<VFramebufferMap::RenderPass> VFramebufferMap::findRenderpass(const VkRenderingInfo* info, const VkPipelineRenderingCreateInfoKHR* fbo) {
  RenderPassDesc desc(*info, *fbo);
  std::lock_guard<std::mutex> guard(syncRp);
  for(auto& i:rp)
    if(i->isSame(desc))
      return i;
  auto ret = std::make_shared<RenderPass>();
  rp.push_back(ret);
  try {
    ret->pass = mkRenderPass(info, fbo);
    }
  catch(...) {
    rp.pop_back();
    throw;
    }
  ret->desc = desc;
  return ret;
  }

VkRenderPass VFramebufferMap::mkRenderPass(const VkRenderingInfo* info, const VkPipelineRenderingCreateInfoKHR* fbo) {
  VkAttachmentDescription attach[MaxFramebufferAttachments] = {};

  for(uint8_t i=0; i<info->colorAttachmentCount; ++i) {
    VkAttachmentDescription& a = attach[i];

    a.format         = fbo->pColorAttachmentFormats[i];
    a.samples        = VK_SAMPLE_COUNT_1_BIT;
    a.loadOp         = info->pColorAttachments[i].loadOp;
    a.storeOp        = info->pColorAttachments[i].storeOp;
    a.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    const bool init = (a.loadOp==VK_ATTACHMENT_LOAD_OP_LOAD);
    a.initialLayout  = init  ? info->pColorAttachments[i].imageLayout : VK_IMAGE_LAYOUT_UNDEFINED;
    a.finalLayout    = info->pColorAttachments[i].imageLayout;
    }

  if(info->pDepthAttachment!=nullptr) {
    VkAttachmentDescription& a = attach[info->colorAttachmentCount];

    a.format         = fbo->depthAttachmentFormat;
    a.samples        = VK_SAMPLE_COUNT_1_BIT;
    a.loadOp         = info->pDepthAttachment->loadOp;
    a.storeOp        = info->pDepthAttachment->storeOp;
    // Stencil is not implemented
    a.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    const bool init = (a.loadOp==VK_ATTACHMENT_LOAD_OP_LOAD);
    a.initialLayout  = init  ? info->pDepthAttachment->imageLayout : VK_IMAGE_LAYOUT_UNDEFINED;
    a.finalLayout    = info->pDepthAttachment->imageLayout;
    }

  VkAttachmentReference   ref[MaxFramebufferAttachments] = {};
  VkAttachmentReference   zs                             = {};
  for(uint8_t i=0; i<info->colorAttachmentCount; ++i) {
    ref[i].attachment = i;
    ref[i].layout     = attach[i].finalLayout;
    }
  if(info->pDepthAttachment!=nullptr) {
    zs.attachment = info->colorAttachmentCount;
    zs.layout     = attach[info->colorAttachmentCount].finalLayout;
    }

  VkSubpassDescription subpass    = {};
  subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount    = info->colorAttachmentCount;
  subpass.pColorAttachments       = ref;
  subpass.pDepthStencilAttachment = (info->pDepthAttachment!=nullptr) ? &zs : nullptr;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = info->colorAttachmentCount + (info->pDepthAttachment!=nullptr ? 1 : 0);
  renderPassInfo.pAttachments    = attach;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 0;
  renderPassInfo.pDependencies   = nullptr;

  VkRenderPass ret = VK_NULL_HANDLE;
  vkAssert(vkCreateRenderPass(dev.device.impl,&renderPassInfo,nullptr,&ret));
  return ret;
  }

VkFramebuffer VFramebufferMap::mkFramebuffer(const VkImageView* attach, size_t attCount, VkExtent2D size, VkRenderPass rp) {
  VkFramebufferCreateInfo crt={};
  crt.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  crt.renderPass      = rp;
  crt.pAttachments    = attach;
  crt.attachmentCount = uint32_t(attCount);
  crt.width           = size.width;
  crt.height          = size.height;
  crt.layers          = 1;

  VkFramebuffer ret=VK_NULL_HANDLE;
  vkAssert(vkCreateFramebuffer(dev.device.impl,&crt,nullptr,&ret));
  return ret;
  }

#endif

