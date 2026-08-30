#include "temporalscaler.h"

using namespace Tempest;

TemporalScaler::TemporalScaler(TemporalScaler&& other) noexcept
  :impl(other.impl.handler) {
  other.impl.handler = nullptr;
  }

TemporalScaler::~TemporalScaler() {
  delete impl.handler;
  }

TemporalScaler& TemporalScaler::operator=(TemporalScaler&& other) noexcept {
  if(this==&other)
    return *this;
  delete impl.handler;
  impl.handler       = other.impl.handler;
  other.impl.handler = nullptr;
  return *this;
  }
