#include "fence.h"
#include <Tempest/Device>

using namespace Tempest;

Fence::Fence(Device &owner)
  :Fence(owner.createFence()) {
  }

Fence::Fence(Device &dev, AbstractGraphicsApi::Fence *impl)
  :dev(&dev),impl(impl) {
  }

Fence::~Fence() {
  dev->destroy(*this);
  }

void Fence::wait() {
  impl.handler->wait();
  }

void Fence::reset() {
  impl.handler->reset();
  }
