#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDevice;

class VCommandPool : public AbstractGraphicsApi::CmdPool {
  public:
    VCommandPool(VDevice &device);
    VCommandPool(VCommandPool&& other);
    ~VCommandPool();

    VkCommandPool impl=VK_NULL_HANDLE;

  private:
    VkDevice      device=nullptr;
  };

}}
