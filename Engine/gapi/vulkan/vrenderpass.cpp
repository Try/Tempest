#include "vrenderpass.h"

#include "vdevice.h"
#include "vswapchain.h"

using namespace Tempest::Detail;

VRenderPass::VRenderPass(VDevice& device,
                         VkFormat format, VkFormat zformat,
                         FboMode color, const Color *clear,
                         FboMode zbuf,  const float *zclear)
  :device(device.device) {
  VkAttachmentDescription attach[2]={};
  VkAttachmentDescription& colorAttachment = attach[0];
  colorAttachment.format         = format;
  colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp         = bool(color&FboMode::PreserveIn)  ? VK_ATTACHMENT_LOAD_OP_LOAD   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.storeOp        = bool(color&FboMode::PreserveOut) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  colorAttachment.initialLayout  = bool(color&FboMode::PreserveIn)  ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
    }

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

  VkSubpassDependency dependency = {};
  dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
  dependency.srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;//VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

  dependency.dstSubpass    = 0;
  dependency.dstStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;//VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

  dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  attachCount = (zformat==VK_FORMAT_UNDEFINED ? 1 : 2);

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = attachCount;
  renderPassInfo.pAttachments    = &colorAttachment;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies   = &dependency;

  vkAssert(vkCreateRenderPass(device.device,&renderPassInfo,nullptr,&impl));
  }

VRenderPass::VRenderPass(VRenderPass &&other) {
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  }

VRenderPass::~VRenderPass(){
  if(device==nullptr)
    return;
  vkDeviceWaitIdle(device);
  vkDestroyRenderPass(device,impl,nullptr);
  }

void VRenderPass::operator=(VRenderPass &&other) {
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  }
