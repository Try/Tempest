#include "fence.h"
#include <Tempest/Device>

using namespace Tempest;

Fence::Fence(AbstractGraphicsApi::Fence *impl)
  :impl(impl) {
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
