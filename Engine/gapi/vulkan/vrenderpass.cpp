#include "vrenderpass.h"

#include "vdevice.h"
#include "vframebufferlayout.h"
#include "vswapchain.h"

#include <Tempest/RenderPass>
#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

VRenderPass::VRenderPass(VDevice& device, const FboMode **attach, uint8_t attCount)
  : attCount(attCount), device(device.device) {
  input.reset(new FboMode[attCount]);
  for(size_t i=0;i<attCount;++i)
    input[i] = *attach[i];
  }

VRenderPass::VRenderPass(VRenderPass &&other) {
  std::swap(attCount,other.attCount);
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  std::swap(input,other.input);
  }

VRenderPass::~VRenderPass(){
  if(device==nullptr)
    return;
  for(auto& i:impl)
    if(i.impl!=VK_NULL_HANDLE)
      vkDestroyRenderPass(device,i.impl,nullptr);
  }

void VRenderPass::operator=(VRenderPass &&other) {
  std::swap(attCount,other.attCount);
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  std::swap(input,other.input);
  }

VRenderPass::Impl &VRenderPass::instance(VFramebufferLayout &lay) {
  std::lock_guard<SpinLock> guard(sync);

  for(auto& i:impl)
    if(i.isCompatible(lay))
      return i;

  VkRenderPass val=VK_NULL_HANDLE;
  try {
    std::unique_ptr<VkClearValue[]> clear(new VkClearValue[attCount]());
    for(size_t i=0;i<attCount;++i) {
      auto& cl = input[i].clear;
      if(Detail::nativeIsDepthFormat(lay.frm[i])){
        clear[i].depthStencil.depth   = cl.r();
        clear[i].depthStencil.stencil = 0;
        } else {
        clear[i].color.float32[0] = cl.r();
        clear[i].color.float32[1] = cl.g();
        clear[i].color.float32[2] = cl.b();
        clear[i].color.float32[3] = cl.a();
        }
      }

    val = createInstance(device,lay.swapchain.get(),input.get(),lay.frm.get(),attCount);
    impl.emplace_back(lay,val,std::move(clear));
    }
  catch(...) {
    if(val!=VK_NULL_HANDLE)
      vkDestroyRenderPass(device,val,nullptr);
    throw;
    }
  return impl.back();
  }

VkRenderPass VRenderPass::createInstance(VkDevice &device, VSwapchain** sw,
                                         const FboMode *att, const VkFormat* format, uint8_t attCount) {
  VkRenderPass ret=VK_NULL_HANDLE;

  VkAttachmentDescription attach[256] = {};
  VkAttachmentReference   ref[256]    = {};
  VkAttachmentReference   zs          = {};

  VkSubpassDescription subpass    = {};
  subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pColorAttachments       = ref;
  subpass.pDepthStencilAttachment = nullptr;

  for(uint8_t i=0; i<attCount; ++i){
    VkAttachmentDescription& a = attach[i];

    if(format[i]==VK_FORMAT_UNDEFINED)
      a.format = sw[i]->format(); else
      a.format = format[i];
    a.samples = VK_SAMPLE_COUNT_1_BIT;

    const FboMode& x = att[i];
    a.loadOp  = bool(x.mode&FboMode::PreserveIn)  ? VK_ATTACHMENT_LOAD_OP_LOAD   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.storeOp = bool(x.mode&FboMode::PreserveOut) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    if(bool(x.mode&FboMode::ClearBit)) {
      a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      }
    // Stencil not implemented
    a.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    const bool init = (a.loadOp==VK_ATTACHMENT_LOAD_OP_LOAD);
    // Note: finalLayout must not be VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED
    // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-VkAttachmentDescription-finalLayout-00843
    if(Detail::nativeIsDepthFormat(a.format)) {
      a.initialLayout = init  ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
      a.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      } else {
      a.initialLayout  = init  ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
      a.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }

    if(Detail::nativeIsDepthFormat(a.format)) {
      VkAttachmentReference& r = zs;
      r.attachment = i;
      r.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      subpass.pDepthStencilAttachment = &r;
      } else {
      VkAttachmentReference& r = ref[i];
      r.attachment = i;
      r.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      subpass.colorAttachmentCount++;
      }
    }

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = attCount;
  renderPassInfo.pAttachments    = attach;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 0;
  renderPassInfo.pDependencies   = nullptr;

  vkAssert(vkCreateRenderPass(device,&renderPassInfo,nullptr,&ret));
  return ret;
  }

VkRenderPass VRenderPass::createLayoutInstance(VkDevice& device,VSwapchain** sw, const VkFormat *att, uint8_t attCount) {
  VkRenderPass ret=VK_NULL_HANDLE;

  VkAttachmentDescription attach[256] = {};
  VkAttachmentReference   ref[256]    = {};
  VkAttachmentReference   zs          = {};

  VkSubpassDescription subpass    = {};
  subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pColorAttachments       = ref;
  subpass.pDepthStencilAttachment = nullptr;

  for(uint8_t i=0;i<attCount;++i){
    VkAttachmentDescription& a = attach[i];
    if(att[i]==VK_FORMAT_UNDEFINED)
      a.format = sw[i]->format(); else
      a.format = att[i];
    a.samples        = VK_SAMPLE_COUNT_1_BIT;

    a.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    if(Detail::nativeIsDepthFormat(a.format)) {
      zs.attachment = i;
      subpass.pDepthStencilAttachment = &zs;
      a.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      a.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      zs.layout       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      } else {
      VkAttachmentReference& r = ref[subpass.colorAttachmentCount];
      subpass.colorAttachmentCount++;
      a.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
      a.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      r.layout         = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }
    }

  VkSubpassDependency dependency = {};
  dependency.srcSubpass      = VK_SUBPASS_EXTERNAL;
  dependency.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
  dependency.dstSubpass      = 0;
  dependency.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = attCount;
  renderPassInfo.pAttachments    = attach;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies   = &dependency;

  vkAssert(vkCreateRenderPass(device,&renderPassInfo,nullptr,&ret));
  return ret;
  }

bool VRenderPass::isAttachPreserved(size_t att) const {
  return (input[att].mode & FboMode::PreserveIn);
  }

bool VRenderPass::Impl::isCompatible(VFramebufferLayout &l) const {
  if(!lay.handler->isCompatible(l))
    return false;
  return true;
  }
