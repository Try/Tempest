#include "mtsync.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtSync::MtSync() {
  }

MtSync::~MtSync() {
  }

void MtSync::wait() {
  if(!hasWait.load())
    return;
  std::unique_lock<std::mutex> guard;
  cv.wait(guard,[this](){ return !hasWait.load(); });
  }

bool MtSync::wait(uint64_t time) {
  if(!hasWait.load())
    return true;
  std::unique_lock<std::mutex> guard;
  return cv.wait_for(guard,std::chrono::milliseconds(time),[this](){ return !hasWait.load(); });
  }

void MtSync::reset() {
  hasWait.store(false);
  cv.notify_all();
  }

void MtSync::signal() {
  hasWait.store(true);
  cv.notify_all();
  }
