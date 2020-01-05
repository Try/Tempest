#include "semaphore.h"
#include <Tempest/Device>

using namespace Tempest;

Semaphore::Semaphore(Device &dev, AbstractGraphicsApi::Semaphore *impl)
  :dev(&dev),impl(impl) {
  }

Semaphore::~Semaphore() {
  delete impl.handler;
  }
