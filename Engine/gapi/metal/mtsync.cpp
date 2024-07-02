#if defined(TEMPEST_BUILD_METAL)

#include "mtsync.h"

#include <Tempest/Except>
#include <Tempest/Log>

using namespace Tempest;
using namespace Tempest::Detail;

MtSync::MtSync() {
  }

MtSync::~MtSync() {
  }

void MtSync::wait() {
  if(!hasWait.load()) {
    propogateError();
    return;
    }
  std::unique_lock<std::mutex> guard(sync);
  cv.wait(guard,[this](){ return !hasWait.load(); });
  propogateError();
  }

bool MtSync::wait(uint64_t time) {
  if(!hasWait.load()) {
    propogateError();
    return true;
    }
  std::unique_lock<std::mutex> guard(sync);
  const bool ret = cv.wait_for(guard,std::chrono::milliseconds(time),[this](){ return !hasWait.load(); });
  propogateError();
  return ret;
  }

void MtSync::reset() {
  hasWait.store(false);
  cv.notify_all();
  }

void MtSync::reset(MTL::CommandBufferStatus err, MTL::CommandBufferError errC, NS::Error* desc) {
  std::unique_lock<std::mutex> guard(sync);
  errorCategory = errC;
  status        = err;
  errorStr      = desc->description()->cString(NS::UTF8StringEncoding);

  errorLog.clear();
  if(auto at = desc->userInfo()->object(MTL::CommandBufferEncoderInfoErrorKey)) {
    auto info = reinterpret_cast<NS::Array*>(at);
    for(size_t i=0; i<info->count(); ++i) {
      auto ix  = reinterpret_cast<MTL::CommandBufferEncoderInfo*>(info->object(i));
      auto str = ix->debugDescription()->cString(NS::UTF8StringEncoding);
      if(!errorLog.empty())
        errorLog += "\n";
      errorLog += str;
      }
    }

  hasWait.store(false);
  cv.notify_all();
  }

void MtSync::signal() {
  hasWait.store(true);
  cv.notify_all();
  }

void MtSync::propogateError() {
  if(status!=MTL::CommandBufferStatusError)
    return;
  if(errorCategory==MTL::CommandBufferErrorTimeout)
    throw DeviceHangException();
  throw DeviceLostException(errorStr, errorLog);
  }

#endif
