#include "vsemaphore.h"

#include "vdevice.h"

using namespace Tempest::Detail;

VSemaphore::VSemaphore(VDevice &device)
  :device(device.device){
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  if(vkCreateSemaphore(device.device,&semaphoreInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::runtime_error("failed to create synchronization objects for a frame!");
  }

VSemaphore::VSemaphore(VSemaphore &&other) {
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  }

VSemaphore::~VSemaphore() {
  if(device==nullptr)
    return;
  vkDeviceWaitIdle(device);
  vkDestroySemaphore(device,impl,nullptr);
  }

void VSemaphore::operator=(VSemaphore &&other) {
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  }
