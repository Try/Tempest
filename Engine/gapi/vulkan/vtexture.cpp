#include "vtexture.h"

#include <Tempest/Pixmap>
#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VTexture::VTexture(VTexture &&other) {
  std::swap(impl,                other.impl);
  std::swap(view,                other.view);
  std::swap(fboView,             other.fboView);
  std::swap(hasDedicatedFboView, other.hasDedicatedFboView);
  std::swap(mipCount,            other.mipCount);
  std::swap(alloc,               other.alloc);
  std::swap(page,                other.page);
  }

VTexture::~VTexture() {
  if(alloc!=nullptr)
    alloc->free(*this);
  }

void VTexture::createViews(VkDevice device, VkFormat format, uint32_t mips) {
  mipCount = mips;
  createView(view,    device, format, mipCount, false);
  if(format==VK_FORMAT_R8_UNORM   || format==VK_FORMAT_R16_UNORM ||
     format==VK_FORMAT_R8G8_UNORM || format==VK_FORMAT_R16G16_UNORM) {
    createView(fboView, device, format, mipCount, true);
    hasDedicatedFboView = true;
    } else {
    fboView = view;
    }
  }

void VTexture::createView(VkImageView& ret, VkDevice device, VkFormat format, uint32_t mips, bool identity) {
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image    = impl;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format   = format;

  if(!identity) {
    if(format==VK_FORMAT_R8_UNORM || format==VK_FORMAT_R16_UNORM) {
      viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
      viewInfo.components.g = VK_COMPONENT_SWIZZLE_R;
      viewInfo.components.b = VK_COMPONENT_SWIZZLE_R;
      viewInfo.components.a = VK_COMPONENT_SWIZZLE_ONE;
      } else
    if(format==VK_FORMAT_R8G8_UNORM || format==VK_FORMAT_R16G16_UNORM) {
      viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
      viewInfo.components.g = VK_COMPONENT_SWIZZLE_R;
      viewInfo.components.b = VK_COMPONENT_SWIZZLE_R;
      viewInfo.components.a = VK_COMPONENT_SWIZZLE_G;
      } else {
      viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
      viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
      viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
      viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
      }
    }

  if(VK_FORMAT_D16_UNORM<=format && format<=VK_FORMAT_D32_SFLOAT_S8_UINT)
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; else
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = mips;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = 1;

  vkAssert(vkCreateImageView(device, &viewInfo, nullptr, &ret));
  }
