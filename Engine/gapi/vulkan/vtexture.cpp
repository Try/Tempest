#include "vtexture.h"

#include <Tempest/Pixmap>
#include "vdevice.h"

using namespace Tempest::Detail;

VTexture::VTexture(VTexture &&other) {
  std::swap(impl,   other.impl);
  std::swap(view,   other.view);
  std::swap(sampler,other.sampler);
  std::swap(alloc,  other.alloc);
  std::swap(page,   other.page);
  }

VTexture::~VTexture() {
  if(alloc!=nullptr)
    alloc->free(*this);
  }

void VTexture::createView(VkDevice device,VkFormat format) {
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image    = impl;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format   = format;
  viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = 1;

  vkAssert(vkCreateImageView(device, &viewInfo, nullptr, &view));
  createTextureSampler(device);
  }

void VTexture::createTextureSampler(VkDevice device) {
  VkSamplerCreateInfo samplerInfo = {};
  samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

  samplerInfo.magFilter               = VK_FILTER_LINEAR;
  samplerInfo.minFilter               = VK_FILTER_LINEAR;
  samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  //samplerInfo.anisotropyEnable        = VK_TRUE;
  //samplerInfo.maxAnisotropy           = 16;
  samplerInfo.maxAnisotropy           = 1.0;
  samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable           = VK_FALSE;
  samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  vkAssert(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));
  }
