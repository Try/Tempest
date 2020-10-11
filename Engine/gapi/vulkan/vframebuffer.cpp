#include "vframebuffer.h"

#include "vdevice.h"
#include "vframebufferlayout.h"
#include "vrenderpass.h"
#include "vswapchain.h"
#include "vtexture.h"

using namespace Tempest::Detail;

VFramebuffer::VFramebuffer(VDevice& device, VFramebufferLayout& lay,
                           uint32_t w, uint32_t h, uint32_t outCnt,
                           VSwapchain** swapchain, VTexture** color, const uint32_t* imgId,
                           VTexture* zbuf)
  :device(device.device) {
  rp = Detail::DSharedPtr<VFramebufferLayout*>(&lay);
  attach.resize(outCnt+(zbuf!=nullptr ? 1 : 0));
  for(uint32_t i=0; i<outCnt; ++i) {
    if(color[i]==nullptr)
      attach[i] = Attach(swapchain[i],imgId[i]); else
      attach[i] = Attach(color[i],0);
    }
  if(zbuf!=nullptr)
    attach.back() = Attach{zbuf,0};

  VkImageView att[256] = {};
  for(uint32_t i=0; i<outCnt; ++i) {
    if(color[i]==nullptr)
      att[i] = swapchain[i]->views[imgId[i]]; else
      att[i] = color[i]->getFboView(device.device,0);
    }
  if(zbuf!=nullptr)
    att[outCnt] = zbuf->view;
  impl = allocFbo(w,h,att,attach.size());
  }

VFramebuffer::~VFramebuffer() {
  if(impl!=VK_NULL_HANDLE)
    vkDestroyFramebuffer(device,impl,nullptr);
  }

VkFramebuffer VFramebuffer::allocFbo(uint32_t w, uint32_t h, const VkImageView *attach, size_t cnt) {
  VkFramebufferCreateInfo crt={};
  crt.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  crt.renderPass      = rp.handler->impl;
  crt.pAttachments    = attach;
  crt.attachmentCount = uint32_t(cnt);
  crt.width           = w;
  crt.height          = h;
  crt.layers          = 1;

  VkFramebuffer ret=VK_NULL_HANDLE;
  if(vkCreateFramebuffer(device,&crt,nullptr,&ret)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  return ret;
  }

Tempest::TextureLayout VFramebuffer::Attach::defaultLayout() {
  if(sw!=nullptr)
    return TextureLayout::Present;
  if(Detail::nativeIsDepthFormat(tex->format))
    return TextureLayout::DepthAttach; // no readable depth for now
  return TextureLayout::Sampler;
  }

Tempest::TextureLayout VFramebuffer::Attach::renderLayout() {
  if(sw!=nullptr)
    return TextureLayout::ColorAttach;
  if(Detail::nativeIsDepthFormat(tex->format))
    return TextureLayout::DepthAttach;
  return TextureLayout::ColorAttach;
  }

void* VFramebuffer::Attach::nativeHandle() {
  if(sw!=nullptr)
    return reinterpret_cast<void*>(sw->images[id]);
  return reinterpret_cast<void*>(tex->impl);
  }
