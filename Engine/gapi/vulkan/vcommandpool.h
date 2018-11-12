#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VDevice;

class VCommandPool : public AbstractGraphicsApi::CmdPool {
  public:
    VCommandPool(VDevice &device, VkCommandPoolCreateFlags flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VCommandPool(VCommandPool&& other);
    ~VCommandPool();

    VkCommandPool impl=VK_NULL_HANDLE;

  private:
    VkDevice      device=nullptr;
  };

}}
