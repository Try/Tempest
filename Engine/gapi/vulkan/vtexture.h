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
class VFramebufferMap;

class VTexture : public AbstractGraphicsApi::Texture {
  public:
    VTexture()=default;
    VTexture(VTexture &&other);
    ~VTexture();

    VTexture& operator=(const VTexture& other)=delete;

    VkImageView view(const ComponentMapping& m, uint32_t mipLevel, bool is3D);
    VkImageView fboView(uint32_t mip);
    uint32_t    mipCount() const override { return mipCnt; }

    VkImage                impl      = VK_NULL_HANDLE;
    VkImageView            imgView   = VK_NULL_HANDLE;
    VkFormat               format    = VK_FORMAT_UNDEFINED;
    NonUniqResId           nonUniqId = NonUniqResId::I_None;

    uint32_t               mipCnt         = 1;
    VAllocator*            alloc          = nullptr;
    VAllocator::Allocation page           = {};
    bool                   isStorageImage = false;
    bool                   is3D           = false;
    bool                   isFilterable   = false;

  protected:
    void createViews (VkDevice device);
    void destroyViews(VkDevice device);
    void createView  (VkImageView& ret, VkDevice device, VkFormat format,
                    const ComponentMapping* cmap, uint32_t mipLevel, bool is3D);

    struct View {
      ComponentMapping m;
      uint32_t         mip  = uint32_t(0);
      bool             is3D = false;
      VkImageView      v;
      };
    Detail::SpinLock  syncViews;
    std::vector<View> extViews;

    friend class VAllocator;
  };

class VTextureWithFbo : public VTexture {
  public:
    VTextureWithFbo(VTexture&& base);
    ~VTextureWithFbo();

    VFramebufferMap* map = nullptr;
  };

}}
