#include "fence.h"
#include <Tempest/Device>

using namespace Tempest;

Fence::Fence(Device& dev, AbstractGraphicsApi::Fence *impl)
  :dev(&dev),impl(impl) {
  }

Fence::~Fence() {
  delete impl.handler;
  }

void Fence::wait() {
  impl.handler->wait();
  }

bool Fence::wait(uint64_t time) {
  return impl.handler->wait(time);
  }

void Fence::reset() {
  impl.handler->reset();
  }
