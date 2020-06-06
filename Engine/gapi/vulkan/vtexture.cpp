#include "vtexture.h"

#include <Tempest/Pixmap>
#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VTexture::VTexture(VTexture &&other) {
  std::swap(impl,    other.impl);
  std::swap(view,    other.view);
  std::swap(mipCount,other.mipCount);
  std::swap(alloc,   other.alloc);
  std::swap(page,    other.page);
  }

VTexture::~VTexture() {
  if(alloc!=nullptr)
    alloc->free(*this);
  }

void VTexture::createView(VkDevice device,VkFormat format,uint32_t mips) {
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image                           = impl;
  viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format                          = format;
  if(VK_FORMAT_D16_UNORM<=format && format<=VK_FORMAT_D32_SFLOAT_S8_UINT)
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; else
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = mips;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = 1;

  vkAssert(vkCreateImageView(device, &viewInfo, nullptr, &view));
  mipCount = mips;
  }
