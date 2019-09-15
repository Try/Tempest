#include "vrenderpass.h"

#include "vdevice.h"
#include "vswapchain.h"

#include <Tempest/RenderPass>

using namespace Tempest::Detail;

VRenderPass::VRenderPass(VDevice& device, const Attachment **attach, size_t attCount)
  :attachCount(attCount), device(device.device) {
  att.reset(new Tempest::Attachment[attCount]);
  for(size_t i=0;i<attCount;++i)
    att[i] = *attach[i];
  }

VRenderPass::VRenderPass(VRenderPass &&other) {
  std::swap(device,other.device);
  std::swap(inst,other.inst);
  }

VRenderPass::~VRenderPass(){
  if(device==nullptr)
    return;
  vkDeviceWaitIdle(device);
  for(auto& i:inst)
    vkDestroyRenderPass(device,i.impl,nullptr);
  }

void VRenderPass::operator=(VRenderPass &&other) {
  std::swap(device,other.device);
  std::swap(inst,other.inst);
  }

VRenderPass::Inst &VRenderPass::instance() {
  for(auto& i:inst)
    return i;

  Inst i;
  try {
    i.impl = createInstance();
    inst.emplace_back(i);
    return inst.back();
    }
  catch (...) {
    if(i.impl!=VK_NULL_HANDLE)
      vkDestroyRenderPass(device,i.impl,nullptr);
    throw;
    }
  }

VkRenderPass VRenderPass::createInstance() {
  VkRenderPass ret=VK_NULL_HANDLE;
  /*
  VkAttachmentDescription attach[2]={};

  VkAttachmentDescription& colorAttachment = attach[0];
  colorAttachment.format         = format;
  colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp         = bool(color&FboMode::PreserveIn)  ? VK_ATTACHMENT_LOAD_OP_LOAD   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.storeOp        = bool(color&FboMode::PreserveOut) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  colorAttachment.initialLayout  = bool(color&FboMode::PreserveIn) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout    = bool(color&FboMode::PresentOut) ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR          : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription& depthAttachment = attach[1];
  depthAttachment.format         = zformat;
  depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp         = bool(zbuf&FboMode::PreserveIn)  ? VK_ATTACHMENT_LOAD_OP_LOAD   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.storeOp        = bool(zbuf&FboMode::PreserveOut) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;

  depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  if(clear){
    this->color=*clear;
    colorAttachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    }
  if(zclear){
    this->zclear=*zclear;
    depthAttachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    }*/

  /*
  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef = {};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass    = {};
  subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount    = 1;
  subpass.pColorAttachments       = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = zformat==VK_FORMAT_UNDEFINED ? nullptr : &depthAttachmentRef;
  */

  VkAttachmentDescription attach[2] = {};
  VkAttachmentReference   ref[2]    = {};

  VkSubpassDescription subpass    = {};
  subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pColorAttachments       = nullptr;
  subpass.pDepthStencilAttachment = nullptr;

  for(size_t i=0;i<attachCount;++i){
    VkAttachmentDescription& a = attach[i];
    VkAttachmentReference&   r = ref[i];
    Attachment&              x = att[i];

    a.format         = Detail::nativeFormat(x.format);
    a.samples        = VK_SAMPLE_COUNT_1_BIT;

    a.loadOp         = bool(x.mode&FboMode::PreserveIn)  ? VK_ATTACHMENT_LOAD_OP_LOAD   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.storeOp        = bool(x.mode&FboMode::PreserveOut) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;

    if(a.format==VK_FORMAT_UNDEFINED)
      a.format = VK_FORMAT_B8G8R8A8_UNORM; // Hack for swapchain image

    if(bool(x.mode&FboMode::Clear)) {
      a.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
      a.storeOp = VK_ATTACHMENT_STORE_OP_STORE; //FIXME
      }

    // Stencil not implemented
    a.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    r.attachment=i;
    if(Tempest::isDepthFormat(x.format)) {
      subpass.pDepthStencilAttachment = &r;
      a.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      a.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      r.layout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      } else {
      subpass.colorAttachmentCount    = 1; //TODO: multi-render target
      subpass.pColorAttachments       = &r;
      a.initialLayout  = bool(x.mode&FboMode::PreserveIn)          ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
      a.finalLayout    = (x.mode&FboMode::Submit)==FboMode::Submit ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR          : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      r.layout         = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }
    }

  VkSubpassDependency dependency = {};
  dependency.srcSubpass      = VK_SUBPASS_EXTERNAL;
  dependency.srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;//VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

  dependency.dstSubpass      = 0;
  dependency.dstStageMask    = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;//VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask   = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

  dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = attachCount;
  renderPassInfo.pAttachments    = attach;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies   = &dependency;

  vkAssert(vkCreateRenderPass(device,&renderPassInfo,nullptr,&ret));
  return ret;
  }
