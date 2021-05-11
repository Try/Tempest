#if defined(TEMPEST_BUILD_VULKAN)

#include "vfence.h"

#include "vdevice.h"

using namespace Tempest::Detail;

VFence::VFence(VDevice &device)
  :device(device.device.impl) {
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  vkAssert(vkCreateFence(device.device.impl,&fenceInfo,nullptr,&impl));
  }

VFence::~VFence() {
  if(device==nullptr)
    return;
  vkDestroyFence(device,impl,nullptr);
  }

void VFence::wait() {
  vkAssert(vkWaitForFences(device,1,&impl,VK_TRUE,std::numeric_limits<uint64_t>::max()));
  }

bool VFence::wait(uint64_t time) {
  if(time<std::numeric_limits<uint64_t>::max())
    time/=uint64_t(1000*1000); // nano to millis convertion
  VkResult res = vkWaitForFences(device,1,&impl,VK_TRUE,time);
  if(res==VK_TIMEOUT)
    return false;
  vkAssert(res);
  return true;
  }

void VFence::reset() {  
  vkAssert(vkResetFences(device,1,&impl));
  }

#endif
