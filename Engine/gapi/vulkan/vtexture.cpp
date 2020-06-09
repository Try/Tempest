#include "vtexture.h"

#include <Tempest/Pixmap>
#include "vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

VTexture::VTexture(VTexture &&other) {
  std::swap(impl,     other.impl);
  std::swap(view,     other.view);
  std::swap(format,   other.format);
  std::swap(mipCount, other.mipCount);
  std::swap(alloc,    other.alloc);
  std::swap(page,     other.page);
  std::swap(extViews, other.extViews);
  }

VTexture::~VTexture() {
  if(alloc!=nullptr)
    alloc->free(*this);
  }

VkImageView VTexture::getView(VkDevice dev, const ComponentMapping& m) {
  if(m.r==ComponentSwizzle::Identity &&
     m.g==ComponentSwizzle::Identity &&
     m.b==ComponentSwizzle::Identity &&
     m.a==ComponentSwizzle::Identity) {
    return view;
    }

  std::lock_guard<Detail::SpinLock> guard(syncViews);
  for(auto& i:extViews) {
    if(i.m==m)
      return i.v;
    }
  View v;
  createView(v.v,dev,format,&m);
  v.m = m;
  try {
    extViews.push_back(v);
    }
  catch (...) {
    vkDestroyImageView(dev,v.v,nullptr);
    throw;
    }
  return v.v;
  }

void VTexture::createViews(VkDevice device) {
  createView(view, device, format, nullptr);
  }

void VTexture::destroyViews(VkDevice device) {
  vkDestroyImageView(device,view,nullptr);
  for(auto& i:extViews)
    vkDestroyImageView(device,i.v,nullptr);
  }

void VTexture::createView(VkImageView& ret, VkDevice device, VkFormat format,
                          const ComponentMapping* cmap) {
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image    = impl;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format   = format;

  if(cmap!=nullptr) {
    static const VkComponentSwizzle sw[] = {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_A
      };
    viewInfo.components.r = sw[uint8_t(cmap->r)];
    viewInfo.components.g = sw[uint8_t(cmap->g)];
    viewInfo.components.b = sw[uint8_t(cmap->b)];
    viewInfo.components.a = sw[uint8_t(cmap->a)];
    }

  if(VK_FORMAT_D16_UNORM<=format && format<=VK_FORMAT_D32_SFLOAT_S8_UINT)
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; else
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = mipCount;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = 1;

  vkAssert(vkCreateImageView(device, &viewInfo, nullptr, &ret));
  }
