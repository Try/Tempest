#include "vcommandpool.h"

#include "vdevice.h"

#include <algorithm>

using namespace Tempest::Detail;

VCommandPool::VCommandPool(VDevice& device)
  :device(device.device) {
  auto queueFamilyIndices = device.findQueueFamilies();

  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

  vkAssert(vkCreateCommandPool(device.device,&poolInfo,nullptr,&impl));
  }

VCommandPool::VCommandPool(VCommandPool &&other) {
  std::swap(device,other.device);
  std::swap(impl,  other.impl);
  }

VCommandPool::~VCommandPool() {
  if(device==nullptr)
    return;
  vkDeviceWaitIdle(device);
  vkDestroyCommandPool(device,impl,nullptr);
  }
