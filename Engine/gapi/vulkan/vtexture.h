#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "vallocator.h"
#include "../utility/spinlock.h"

namespace Tempest {

class Pixmap;

namespace Detail {

class VDevice;
class VBuffer;

class VTexture : public AbstractGraphicsApi::Texture {
  public:
    VTexture()=default;
    VTexture(VTexture &&other);
    ~VTexture();

    VTexture& operator=(const VTexture& other)=delete;

    VkImageView getView(VkDevice dev, const ComponentMapping& m, uint32_t mipLevel);
    VkImageView getFboView(VkDevice dev, uint32_t mip);
    uint32_t    mipCount() const override { return mipCnt; }

    VkImage     impl     = VK_NULL_HANDLE;
    VkImageView view     = VK_NULL_HANDLE;
    VkFormat    format   = VK_FORMAT_UNDEFINED;

    uint32_t               mipCnt = 1;
    VAllocator*            alloc =nullptr;
    VAllocator::Allocation page  ={};

  private:
    void createViews (VkDevice device);
    void destroyViews(VkDevice device);
    void createView  (VkImageView& ret, VkDevice device, VkFormat format,
                      const ComponentMapping* cmap, uint32_t mipLevel);

    struct View {
      ComponentMapping m;
      uint32_t         mip = uint32_t(0);
      VkImageView      v;
      };
    Detail::SpinLock  syncViews;
    std::vector<View> extViews;

    friend class VAllocator;
  };

}}
