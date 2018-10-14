#include "semaphore.h"
#include <Tempest/Device>

using namespace Tempest;

Semaphore::Semaphore(Device &owner)
  :Semaphore(owner.createSemaphore()) {
  }

Semaphore::Semaphore(Device &dev, AbstractGraphicsApi::Semaphore *impl)
  :dev(&dev),impl(impl) {
  }

Semaphore::~Semaphore() {
  if(dev)
    dev->destroy(*this);
  }
