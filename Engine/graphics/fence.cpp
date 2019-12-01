#include "fence.h"
#include <Tempest/Device>

using namespace Tempest;

Fence::Fence(HeadlessDevice &dev, AbstractGraphicsApi::Fence *impl)
  :dev(&dev),impl(impl) {
  }

Fence::~Fence() {
  delete impl.handler;
  }

void Fence::wait() {
  impl.handler->wait();
  }

void Fence::reset() {
  impl.handler->reset();
  }
