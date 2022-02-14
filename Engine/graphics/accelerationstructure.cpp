#include "accelerationstructure.h"

using namespace Tempest;

AccelerationStructure::AccelerationStructure(Device& dev, AbstractGraphicsApi::AccelerationStructure* impl)
  :impl(std::move(impl)) {
  }
