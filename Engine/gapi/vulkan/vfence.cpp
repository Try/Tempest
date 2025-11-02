#if defined(TEMPEST_BUILD_VULKAN)

#include "vfence.h"
#include "vdevice.h"

using namespace Tempest::Detail;

void VTimepoint::wait() {
  vkAssert(device->waitFence(*this, std::numeric_limits<uint64_t>::max()));
  }

bool VTimepoint::wait(uint64_t time) {
  VkResult res = device->waitFence(*this, time);
  if(res==VK_NOT_READY)
    return false;
  vkAssert(res);
  return true;
  }

VFence::VFence() {
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
    if(res==VK_NOT_READY)
      return false;
    vkAssert(res);
    return true;
    }
  return true;
  }

#endif
