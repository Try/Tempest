#include "vframebuffer.h"

#include "vdevice.h"
#include "vrenderpass.h"
#include "vswapchain.h"
#include "vtexture.h"

using namespace Tempest::Detail;

VFramebuffer::Inputs::Inputs(VSwapchain &swapchain, size_t image)
  :sw(&swapchain), swImg(image) {
  crt.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  crt.renderPass      = VK_NULL_HANDLE;//renderPass.impl;
  crt.pAttachments    = &swapchain.swapChainImageViews[image];
  crt.attachmentCount = 1;
  crt.width           = swapchain.w();
  crt.height          = swapchain.h();
  crt.layers          = 1;
  }

VFramebuffer::Inputs::Inputs(VSwapchain &swapchain, size_t image, VTexture &zbuf)
  :sw(&swapchain), swImg(image) {
  tx[0] = DSharedPtr<VTexture*>(&zbuf);

  VkImageView attach[2] = {swapchain.swapChainImageViews[image],zbuf.view};

  crt.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  crt.renderPass      = VK_NULL_HANDLE;//renderPass.impl;
  crt.pAttachments    = attach;
  crt.attachmentCount = 2;
  crt.width           = swapchain.w();
  crt.height          = swapchain.h();
  crt.layers          = 1;
  }

VFramebuffer::Inputs::Inputs(uint32_t w, uint32_t h, VTexture &color) {
  tx[1] = DSharedPtr<VTexture*>(&color);
  crt.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  //crt.renderPass      = renderPass.impl;
  //crt.pAttachments    = attach;
  crt.attachmentCount = 2;
  crt.width           = w;
  crt.height          = h;
  crt.layers          = 1;
  }

VFramebuffer::Inputs::Inputs(uint32_t w, uint32_t h, VTexture &color, VTexture &zbuf) {
  tx[0] = DSharedPtr<VTexture*>(&zbuf);
  tx[1] = DSharedPtr<VTexture*>(&color);

  crt.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  //crt.renderPass      = renderPass.impl;
  //crt.pAttachments    = attach;
  crt.attachmentCount = 2;
  crt.width           = w;
  crt.height          = h;
  crt.layers          = 1;
  }

VkFramebuffer VFramebuffer::Inputs::alloc(VkDevice device,VkRenderPass renderPass) {
  VkImageView attach[2] = {};

  if(sw!=nullptr){
    if(tx[0].handler!=nullptr){
      crt.attachmentCount = 2;
      attach[0] = sw->swapChainImageViews[swImg];
      attach[1] = tx[0].handler->view;
      } else {
      crt.attachmentCount = 1;
      attach[0] = sw->swapChainImageViews[swImg];
      }
    } else {
    if(tx[0].handler!=nullptr){
      crt.attachmentCount = 2;
      attach[0] = tx[1].handler->view;
      attach[1] = tx[0].handler->view;
      } else {
      crt.attachmentCount = 1;
      attach[0] = tx[1].handler->view;
      }
    }

  crt.renderPass   = renderPass;
  crt.pAttachments = attach;

  VkFramebuffer ret=VK_NULL_HANDLE;
  if(vkCreateFramebuffer(device,&crt,nullptr,&ret)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  return ret;
  }

VFramebuffer::VFramebuffer(VDevice& device,VSwapchain& swapchain,size_t image)
  :device(device.device),inputs(swapchain,image) {
  }

VFramebuffer::VFramebuffer(VDevice &device, VSwapchain &swapchain, size_t image, VTexture &zbuf)
  :device(device.device),inputs(swapchain,image,zbuf) {
  }

VFramebuffer::VFramebuffer(VDevice &device, uint32_t w, uint32_t h, VTexture &color, VTexture &zbuf)
  :device(device.device), inputs(w,h,color,zbuf) {
  }

VFramebuffer::VFramebuffer(VDevice &device, uint32_t w, uint32_t h, VTexture &color)
  :device(device.device), inputs(w,h,color) {
  }

VFramebuffer::VFramebuffer(VFramebuffer &&other)
  :inputs(other.inputs) {
  std::swap(inst,other.inst);
  std::swap(device,other.device);
  }

VFramebuffer::~VFramebuffer() {
  vkDeviceWaitIdle(device);
  for(auto& i:inst)
    vkDestroyFramebuffer(device,i.impl,nullptr);
  }

void VFramebuffer::operator=(VFramebuffer &&other) {
  std::swap(inst,  other.inst);
  std::swap(device,other.device);
  std::swap(inputs,other.inputs);
  }

VFramebuffer::Inst &VFramebuffer::instance(VRenderPass &pass) {
  for(auto& i:inst)
    if(i.rp.handler==&pass)
      return i;
  VkFramebuffer val=VK_NULL_HANDLE;
  try {
    VkRenderPass p = pass.instance().impl;
    val = inputs.alloc(device,p);
    inst.emplace_back(&pass,val);
    }
  catch(...) {
    if(val!=VK_NULL_HANDLE)
      vkDestroyFramebuffer(device,val,nullptr);
    throw;
    }
  return inst.back();
  }
