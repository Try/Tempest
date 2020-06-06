#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "vallocator.h"

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

    VkImage     impl    =VK_NULL_HANDLE;
    VkImageView view    =VK_NULL_HANDLE;
    uint32_t    mipCount=1;

    VAllocator*            alloc =nullptr;
    VAllocator::Allocation page  ={};

  private:
    void                   createView(VkDevice device, VkFormat format, uint32_t mipCount);

    friend class VAllocator;
  };

}}
