#if defined(TEMPEST_BUILD_METAL)

#include "mtsync.h"

#include <Tempest/Except>

using namespace Tempest;
using namespace Tempest::Detail;

MtSync::MtSync() {
  }

MtSync::~MtSync() {
  }

void MtSync::wait() {
  if(!hasWait.load())
    return;
  std::unique_lock<std::mutex> guard(sync);
  cv.wait(guard,[this](){ return !hasWait.load(); });
  propogateError();
  }

bool MtSync::wait(uint64_t time) {
  if(!hasWait.load())
    return true;
  std::unique_lock<std::mutex> guard(sync);
  const bool ret = cv.wait_for(guard,std::chrono::milliseconds(time),[this](){ return !hasWait.load(); });
  propogateError();
  return ret;
  }

void MtSync::reset() {
  hasWait.store(false);
  cv.notify_all();
  }

void MtSync::reset(MTL::CommandBufferStatus err) {
  std::unique_lock<std::mutex> guard(sync);
  status = err;
  hasWait.store(false);
  cv.notify_all();
  }

void MtSync::signal() {
  hasWait.store(true);
  cv.notify_all();
  }

void MtSync::propogateError() {
  if(status==MTL::CommandBufferStatusError)
    throw DeviceLostException();
  }

#endif
