#if defined(TEMPEST_BUILD_VULKAN)

#include "vframebuffermap.h"

#include "vdevice.h"
#include "vtexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

bool VFramebufferMap::RenderPass::isCompatible(const RenderPass& other) const {
  if(descSize!=other.descSize)
    return false;
  for(size_t i=0; i<descSize; ++i)
    if(desc[i].frm!=other.desc[i].frm)
      return false;
  return true;
  }

bool VFramebufferMap::RenderPass::isSame(const Desc* d, size_t cnt) const {
  return
      descSize==cnt &&
      std::memcmp(desc,d,cnt*sizeof(Desc))==0;
  }

bool VFramebufferMap::Fbo::isSame(const Desc* d, const VkImageView* v, size_t cnt) const {
  return
      descSize==cnt &&
      std::memcmp(pass->desc,d,cnt*sizeof(Desc))==0 &&
      std::memcmp(view,v,cnt*sizeof(VkImageView))==0;
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
  for(size_t i=0; i<val.size(); ++i) {
    auto& v = *val[i];
    if(!v.hasImg(img))
      continue;
    vkDestroyFramebuffer(device,v.fbo,nullptr);
    val[i] = std::move(val.back());
    val.pop_back();
    }
  }

std::shared_ptr<VFramebufferMap::Fbo> VFramebufferMap::find(const AttachmentDesc* desc, size_t descSize,
                                                            const TextureFormat* frm,
                                                            AbstractGraphicsApi::Texture** att, AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId,
                                                            uint32_t w, uint32_t h) {
  Desc        dx  [MaxFramebufferAttachments];
  VkImageView view[MaxFramebufferAttachments] = {};

  for(size_t i=0; i<descSize; ++i) {
    auto& d = dx[i];

    d.load  = desc[i].load;
    d.store = desc[i].store;
    if(sw[i]!=nullptr)
      d.frm = reinterpret_cast<VSwapchain*>(sw[i])->format(); else
      d.frm = nativeFormat(frm[i]);

    if(sw[i]!=nullptr)
      view[i] = reinterpret_cast<VSwapchain*>(sw[i])->views[imgId[i]]; else
      view[i] = reinterpret_cast<VTexture*>(att[i])->fboView(dev.device.impl,0);
    }

  std::lock_guard<std::mutex> guard(syncFbo);
  for(auto& i:val)
    if(i->isSame(dx,view,descSize))
      return i;

  for(size_t i=0; i<descSize; ++i) {
    if(sw[i]!=nullptr) {
      auto& s = *reinterpret_cast<VSwapchain*>(sw[i]);
      s.map = this;
      } else {
      auto& t = *reinterpret_cast<VTextureWithFbo*>(att[i]);
      t.map = this;
      }
    }

  val.push_back(std::make_shared<Fbo>());
  try {
    *val.back() = mkFbo(dx,view,descSize,w,h);
    }
  catch(...) {
    val.pop_back();
    throw;
    }
  return val.back();
  }

VFramebufferMap::Fbo VFramebufferMap::mkFbo(const Desc* desc, const VkImageView* view, size_t attCount, uint32_t w, uint32_t h) {
  Fbo ret = {};
  ret.pass = findRenderpass(desc,attCount);
  ret.fbo  = mkFramebuffer(view,attCount,w,h,ret.pass->pass);

  std::memcpy(ret.view,view,attCount*sizeof(VkImageView));
  ret.descSize = uint8_t(attCount);
  return ret;
  }

std::shared_ptr<VFramebufferMap::RenderPass> VFramebufferMap::findRenderpass(const Desc* desc, size_t cnt) {
  std::lock_guard<std::mutex> guard(syncRp);
  for(auto& i:rp)
    if(i->isSame(desc,cnt))
      return i;
  auto ret = std::make_shared<RenderPass>();
  rp.push_back(ret);
  try {
    ret->pass = mkRenderPass(desc,cnt);
    }
  catch(...) {
    rp.pop_back();
    throw;
    }
  ret->descSize = uint8_t(cnt);
  std::memcpy(ret->desc,desc,cnt*sizeof(Desc));
  return ret;
  }

VkRenderPass VFramebufferMap::mkRenderPass(const Desc* desc, size_t attCount) {
  VkAttachmentDescription attach[MaxFramebufferAttachments] = {};
  VkAttachmentReference   ref[MaxFramebufferAttachments]    = {};
  VkAttachmentReference   zs                                = {};

  VkSubpassDescription subpass    = {};
  subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pColorAttachments       = ref;
  subpass.pDepthStencilAttachment = nullptr;
  subpass.colorAttachmentCount    = 0;
  for(uint8_t i=0; i<attCount; ++i) {
    VkAttachmentDescription& a = attach[i];
    a.format  = desc[i].frm;
    a.samples = VK_SAMPLE_COUNT_1_BIT;

    const Desc& x = desc[i];
    a.loadOp  = (x.load ==AccessOp::Preserve) ? VK_ATTACHMENT_LOAD_OP_LOAD   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.storeOp = (x.store==AccessOp::Preserve) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    if(x.load == AccessOp::Clear)
      a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

    // Stencil is not implemented
    a.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    const bool init = (a.loadOp==VK_ATTACHMENT_LOAD_OP_LOAD);
    // Note: finalLayout must not be VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED
    // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-VkAttachmentDescription-finalLayout-00843
    if(Detail::nativeIsDepthFormat(a.format)) {
      a.initialLayout = init  ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
      a.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      } else {
      a.initialLayout = init  ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
      a.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }

    if(Detail::nativeIsDepthFormat(a.format)) {
      VkAttachmentReference& r = zs;
      r.attachment = i;
      r.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      subpass.pDepthStencilAttachment = &r;
      } else {
      VkAttachmentReference& r = ref[subpass.colorAttachmentCount];
      r.attachment = i;
      r.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      subpass.colorAttachmentCount++;
      }
    }

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = uint32_t(attCount);
  renderPassInfo.pAttachments    = attach;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 0;
  renderPassInfo.pDependencies   = nullptr;

  VkRenderPass ret=VK_NULL_HANDLE;
  vkAssert(vkCreateRenderPass(dev.device.impl,&renderPassInfo,nullptr,&ret));
  return ret;
  }

VkFramebuffer VFramebufferMap::mkFramebuffer(const VkImageView* attach, size_t attCount, uint32_t w, uint32_t h, VkRenderPass rp) {
  VkFramebufferCreateInfo crt={};
  crt.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  crt.renderPass      = rp;
  crt.pAttachments    = attach;
  crt.attachmentCount = uint32_t(attCount);
  crt.width           = w;
  crt.height          = h;
  crt.layers          = 1;

  VkFramebuffer ret=VK_NULL_HANDLE;
  vkAssert(vkCreateFramebuffer(dev.device.impl,&crt,nullptr,&ret));
  return ret;
  }

#endif
