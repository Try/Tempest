#include "semaphore.h"
#include <Tempest/Device>

using namespace Tempest;

Semaphore::Semaphore(HeadlessDevice &owner)
  :Semaphore(owner.createSemaphore()) {
  }

Semaphore::Semaphore(HeadlessDevice &dev, AbstractGraphicsApi::Semaphore *impl)
  :dev(&dev),impl(impl) {
  }

Semaphore::~Semaphore() {
  delete impl.handler;
  }
