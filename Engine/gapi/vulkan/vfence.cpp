#if defined(TEMPEST_BUILD_VULKAN)

#include "vfence.h"
#include "vdevice.h"

using namespace Tempest::Detail;

VFence::VFence(VDevice &device) : device(&device) {
  }

VFence::~VFence() {
  }

void VFence::wait() {
  if(auto t = timepoint.lock()) {
    vkAssert(device->waitFence(*t, std::numeric_limits<uint64_t>::max()));
    }
  }

bool VFence::wait(uint64_t time) {
  if(auto t = timepoint.lock()) {
    VkResult res = device->waitFence(*t, time);
    if(res==VK_TIMEOUT || res==VK_NOT_READY)
      return false;
    vkAssert(res);
    return true;
    }
  return true;
  }

void VFence::reset() {  
  //vkAssert(vkResetFences(device,1,&impl));
  }

#endif
