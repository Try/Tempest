#include "vframebuffer.h"

#include "vdevice.h"
#include "vframebufferlayout.h"
#include "vrenderpass.h"
#include "vswapchain.h"
#include "vtexture.h"

using namespace Tempest::Detail;

VFramebuffer::VFramebuffer(VDevice& device, VFramebufferLayout &lay, VSwapchain& swapchain, size_t image)
  :device(device.device) {
  rp   = Detail::DSharedPtr<VFramebufferLayout*>(&lay);

  VkImageView attach[1] = {swapchain.swapChainImageViews[image]};
  impl = allocFbo(swapchain.w(),swapchain.h(),attach,1);
  }

VFramebuffer::VFramebuffer(VDevice &device, VFramebufferLayout &lay, VSwapchain &swapchain, size_t image, VTexture &zbuf)
  :device(device.device) {
  rp = Detail::DSharedPtr<VFramebufferLayout*>(&lay);

  VkImageView attach[2] = {swapchain.swapChainImageViews[image],zbuf.view};
  impl = allocFbo(swapchain.w(),swapchain.h(),attach,2);
  }

VFramebuffer::VFramebuffer(VDevice &device, VFramebufferLayout &lay, uint32_t w, uint32_t h, VTexture &color, VTexture &zbuf)
  :device(device.device) {
  rp = Detail::DSharedPtr<VFramebufferLayout*>(&lay);

  VkImageView attach[2] = {color.view,zbuf.view};
  impl = allocFbo(w,h,attach,2);
  }

VFramebuffer::VFramebuffer(VDevice &device, VFramebufferLayout &lay, uint32_t w, uint32_t h, VTexture &color)
  :device(device.device) {
  rp = Detail::DSharedPtr<VFramebufferLayout*>(&lay);

  VkImageView attach[1] = {color.view};
  impl = allocFbo(w,h,attach,1);
  }

VFramebuffer::VFramebuffer(VFramebuffer &&other) {
  std::swap(impl,  other.impl);
  std::swap(rp,    other.rp);
  std::swap(device,other.device);
  }

VFramebuffer::~VFramebuffer() {
  vkDeviceWaitIdle(device);
  if(impl!=VK_NULL_HANDLE)
    vkDestroyFramebuffer(device,impl,nullptr);
  }

void VFramebuffer::operator=(VFramebuffer &&other) {
  std::swap(impl,  other.impl);
  std::swap(rp,other.rp);
  std::swap(device,other.device);
  }

VkFramebuffer VFramebuffer::allocFbo(uint32_t w, uint32_t h, const VkImageView *attach, size_t cnt) {
  VkFramebufferCreateInfo crt={};
  crt.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  crt.renderPass      = rp.handler->impl;
  crt.pAttachments    = attach;
  crt.attachmentCount = cnt;
  crt.width           = w;
  crt.height          = h;
  crt.layers          = 1;

  VkFramebuffer ret=VK_NULL_HANDLE;
  if(vkCreateFramebuffer(device,&crt,nullptr,&ret)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  return ret;
  }
