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

    VkImage     impl     = VK_NULL_HANDLE;
    VkImageView view     = VK_NULL_HANDLE;
    VkFormat    format   = VK_FORMAT_UNDEFINED;

    VkImageView getView(VkDevice dev, const ComponentMapping& m);

    uint32_t    mipCount = 1;

    VAllocator*            alloc =nullptr;
    VAllocator::Allocation page  ={};

  private:
    void createViews (VkDevice device);
    void destroyViews(VkDevice device);
    void createView  (VkImageView& ret, VkDevice device, VkFormat format, const ComponentMapping* cmap);

    struct View {
      ComponentMapping m;
      VkImageView      v;
      };
    Detail::SpinLock  syncViews;
    std::vector<View> extViews;

    friend class VAllocator;
  };

}}
