#if defined(TEMPEST_BUILD_METAL)

#include "mtsync.h"

#include "gapi/metal/mtdevice.h"

#include <Tempest/Except>
#include <Tempest/Log>

using namespace Tempest;
using namespace Tempest::Detail;

bool MtTimepoint::isFinalStatus() const {
  return status==MTL::CommandBufferStatusCompleted || status==MTL::CommandBufferStatusError;
  }

MtSync::MtSync() {
  }

MtSync::~MtSync() {
  }

void MtSync::wait() {
  if(auto t = timepoint.lock()) {
    MTL::CommandBufferStatus res = device->waitFence(*t, std::numeric_limits<uint64_t>::max());
    if(res!=MTL::CommandBufferStatusCompleted && res!=MTL::CommandBufferStatusError)
      return;
    propogateError(*t);
    }
  }

bool MtSync::wait(uint64_t time) {
  if(auto t = timepoint.lock()) {
    MTL::CommandBufferStatus res = device->waitFence(*t, time);
    if(res!=MTL::CommandBufferStatusCompleted && res!=MTL::CommandBufferStatusError)
      return false;
    propogateError(*t);
    return true;
    }
  return true;
  }

void MtSync::propogateError(const MtTimepoint& t) {
  if(t.status!=MTL::CommandBufferStatusError)
    return;
  if(t.error==MTL::CommandBufferErrorTimeout)
    throw DeviceHangException();
  throw DeviceLostException(t.errorStr, t.errorLog);
  }

#endif
