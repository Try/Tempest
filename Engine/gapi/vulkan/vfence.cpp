#if defined(TEMPEST_BUILD_VULKAN)

#include "vfence.h"
#include "vdevice.h"

using namespace Tempest::Detail;

void VFence::wait() {
  vkAssert(device->waitFence(*this, std::numeric_limits<uint64_t>::max()));
  }

bool VFence::wait(uint64_t time) {
  VkResult res = device->waitFence(*this, time);
  if(res==VK_NOT_READY)
    return false;
  vkAssert(res);
  return true;
  }

#endif
