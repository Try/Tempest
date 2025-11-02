#if defined(TEMPEST_BUILD_METAL)

#include "mtsync.h"

#include "gapi/metal/mtdevice.h"

#include <Tempest/Except>
#include <Tempest/Log>

using namespace Tempest;
using namespace Tempest::Detail;

void MtTimepoint::wait() {
  MTL::CommandBufferStatus res = device->waitFence(*this, std::numeric_limits<uint64_t>::max());
  if(res!=MTL::CommandBufferStatusCompleted && res!=MTL::CommandBufferStatusError)
    return;
  propogateError(*this);
  }

bool MtTimepoint::wait(uint64_t time) {
  MTL::CommandBufferStatus res = device->waitFence(*this, time);
  if(res!=MTL::CommandBufferStatusCompleted && res!=MTL::CommandBufferStatusError)
    return false;
  propogateError(*this);
  return true;
  }

void MtTimepoint::propogateError(const MtTimepoint& t) {
  if(t.status!=MTL::CommandBufferStatusError)
    return;
  if(t.error==MTL::CommandBufferErrorTimeout)
    throw DeviceHangException();
  throw DeviceLostException(t.errorStr, t.errorLog);
  }

bool MtTimepoint::isFinalStatus() const {
  auto st = status.load();
  return st==MTL::CommandBufferStatusCompleted || st==MTL::CommandBufferStatusError;
  }

void MtTimepoint::clear() {
  status.store(MTL::CommandBufferStatusNotEnqueued);
  error  = MTL::CommandBufferErrorNone;
  errorStr.clear();
  errorLog.clear();
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
