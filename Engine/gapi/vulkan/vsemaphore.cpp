#if defined(TEMPEST_BUILD_VULKAN)

#include "vsemaphore.h"

#include "vdevice.h"

using namespace Tempest::Detail;

VSemaphore::VSemaphore(VDevice &device)
  :device(device.device){
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.flags = 0;

  vkAssert(vkCreateSemaphore(device.device,&semaphoreInfo,nullptr,&impl));
  }

VSemaphore::VSemaphore(VSemaphore &&other) {
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  std::swap(stage,other.stage);
  }

VSemaphore::~VSemaphore() {
  if(device==nullptr)
    return;
  vkDestroySemaphore(device,impl,nullptr);
  }

#endif
