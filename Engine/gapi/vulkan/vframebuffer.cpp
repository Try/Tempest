#include "vframebuffer.h"

#include "vdevice.h"
#include "vrenderpass.h"
#include "vswapchain.h"

using namespace Tempest::Detail;

VFramebuffer::VFramebuffer(VDevice& device,VRenderPass& renderPass,VSwapchain& swapchain,size_t image)
  :device(device.device) {
  VkFramebufferCreateInfo framebufferInfo = {};
  framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass      = renderPass.renderPass;
  framebufferInfo.pAttachments    = &swapchain.swapChainImageViews[image];
  framebufferInfo.attachmentCount = 1;
  framebufferInfo.width           = swapchain.width();
  framebufferInfo.height          = swapchain.height();
  framebufferInfo.layers          = 1;

  if(vkCreateFramebuffer(device.device,&framebufferInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::runtime_error("failed to create framebuffer!");
  }

VFramebuffer::VFramebuffer(VFramebuffer &&other) {
  std::swap(impl,other.impl);
  std::swap(device,other.device);
  }

VFramebuffer::~VFramebuffer() {
  if(impl==VK_NULL_HANDLE)
    return;
  vkDeviceWaitIdle(device);
  vkDestroyFramebuffer(device,impl,nullptr);
  }

void VFramebuffer::operator=(VFramebuffer &&other) {
  std::swap(impl,other.impl);
  std::swap(device,other.device);
  }
