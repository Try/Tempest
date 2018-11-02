#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

#include "vallocator.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VImage : public AbstractGraphicsApi::Image {
  public:
    VImage()=default;
    VImage(VDevice& dev,VkImage impl);
    VImage(VImage&& other);
    ~VImage();

    VkImage  impl              =VK_NULL_HANDLE;
    VkDevice device            =nullptr;
    uint32_t presentQueueFamily=uint32_t(-1);

  private:
    VAllocator*            alloc=nullptr;
    VAllocator::Allocation page={};

  friend class VAllocator;
  };

}}
