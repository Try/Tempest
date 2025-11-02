#if defined(TEMPEST_BUILD_METAL)

#include "mtsync.h"

#include "gapi/metal/mtdevice.h"

#include <Tempest/Except>
#include <Tempest/Log>

using namespace Tempest;
using namespace Tempest::Detail;

void MtFence::wait() {
  MTL::CommandBufferStatus res = device->waitFence(*this, std::numeric_limits<uint64_t>::max());
  if(res!=MTL::CommandBufferStatusCompleted && res!=MTL::CommandBufferStatusError)
    return;
  propogateError();
  }

bool MtFence::wait(uint64_t time) {
  MTL::CommandBufferStatus res = device->waitFence(*this, time);
  if(res!=MTL::CommandBufferStatusCompleted && res!=MTL::CommandBufferStatusError)
    return false;
  propogateError();
  return true;
  }

void MtFence::propogateError() {
  if(status!=MTL::CommandBufferStatusError)
    return;
  if(error==MTL::CommandBufferErrorTimeout)
    throw DeviceHangException();
  throw DeviceLostException(errorStr, errorLog);
  }

bool MtFence::isFinalStatus() const {
  auto st = status.load();
  return st==MTL::CommandBufferStatusCompleted || st==MTL::CommandBufferStatusError;
  }

void MtFence::clear() {
  status.store(MTL::CommandBufferStatusNotEnqueued);
  error  = MTL::CommandBufferErrorNone;
  errorStr.clear();
  errorLog.clear();
  }

#endif
