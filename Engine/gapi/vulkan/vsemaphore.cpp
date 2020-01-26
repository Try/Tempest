#include "vsemaphore.h"

#include "vdevice.h"

using namespace Tempest::Detail;

VSemaphore::VSemaphore(VDevice &device)
  :device(device.device){
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  vkAssert(vkCreateSemaphore(device.device,&semaphoreInfo,nullptr,&impl));
  }

VSemaphore::VSemaphore(VSemaphore &&other) {
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  }

VSemaphore::~VSemaphore() {
  if(device==nullptr)
    return;
  vkDestroySemaphore(device,impl,nullptr);
  }

void VSemaphore::operator=(VSemaphore &&other) {
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  }
