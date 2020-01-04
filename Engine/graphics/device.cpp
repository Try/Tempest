#include "device.h"

#include <Tempest/Semaphore>
#include <Tempest/Except>

using namespace Tempest;

Device::Impl::Impl(uint8_t maxFramesInFlight)
  :maxFramesInFlight(maxFramesInFlight){
  }

Device::Impl::~Impl() {
  }

Device::Device(AbstractGraphicsApi &api, uint8_t maxFramesInFlight)
  :HeadlessDevice(api,nullptr), impl(maxFramesInFlight) {
  }

Device::~Device() {
  }

uint8_t Device::maxFramesInFlight() const {
  return impl.maxFramesInFlight;
  }
