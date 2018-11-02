#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

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
  };

}}
