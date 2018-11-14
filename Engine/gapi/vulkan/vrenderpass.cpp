#include "vrenderpass.h"

#include "vdevice.h"
#include "vswapchain.h"

using namespace Tempest::Detail;

VRenderPass::VRenderPass(VDevice& device, VkFormat format,
                         FboMode in,  const Color *clear,
                         FboMode out, const float zclear)
  :device(device.device) {
  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format         = format;
  colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp         = bool(in&FboMode::PreserveIn)  ? VK_ATTACHMENT_LOAD_OP_LOAD   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.storeOp        = bool(in&FboMode::PreserveOut) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  if(clear){
    color=*clear;
    colorAttachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    }

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments    = &colorAttachmentRef;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass    = 0;
  dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments    = &colorAttachment;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies   = &dependency;

  vkAssert(vkCreateRenderPass(device.device,&renderPassInfo,nullptr,&renderPass));
  }

VRenderPass::VRenderPass(VRenderPass &&other) {
  std::swap(device,other.device);
  std::swap(renderPass,other.renderPass);
  }

VRenderPass::~VRenderPass(){
  if(device==nullptr)
    return;
  vkDeviceWaitIdle(device);
  vkDestroyRenderPass(device,renderPass,nullptr);
  }

void VRenderPass::operator=(VRenderPass &&other) {
  std::swap(device,other.device);
  std::swap(renderPass,other.renderPass);
  }
