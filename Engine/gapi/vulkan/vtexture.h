#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

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

    VkImage     impl   =VK_NULL_HANDLE;
    VkImageView view   =VK_NULL_HANDLE;
    VkSampler   sampler=VK_NULL_HANDLE;

  private:
    VAllocator*            alloc =nullptr;
    VAllocator::Allocation page  ={};

    void                   createView(VkDevice device, VkFormat format);
    void                   createTextureSampler(VkDevice device);

    friend class VAllocator;
  };

}}
