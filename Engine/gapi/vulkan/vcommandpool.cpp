#if defined(TEMPEST_BUILD_VULKAN)

#include "vcommandpool.h"

#include "vdevice.h"

#include <algorithm>

using namespace Tempest::Detail;

VCommandPool::VCommandPool(VDevice& device,VkCommandPoolCreateFlags flags)
  :device(device.device) {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = device.props.graphicsFamily;
  poolInfo.flags            = flags;

  vkAssert(vkCreateCommandPool(device.device,&poolInfo,nullptr,&impl));
  }

VCommandPool::VCommandPool(VCommandPool &&other) {
  std::swap(device,other.device);
  std::swap(impl,  other.impl);
  }

VCommandPool::~VCommandPool() {
  if(device==nullptr)
    return;
  vkDestroyCommandPool(device,impl,nullptr);
  }

#endif
