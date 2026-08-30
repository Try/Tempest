#include "spatialscaler.h"

using namespace Tempest;

SpatialScaler::SpatialScaler(SpatialScaler&& other) noexcept
  :impl(other.impl.handler) {
  other.impl.handler = nullptr;
  }

SpatialScaler::~SpatialScaler() {
  delete impl.handler;
  }

SpatialScaler& SpatialScaler::operator=(SpatialScaler&& other) noexcept {
  if(this==&other)
    return *this;
  delete impl.handler;
  impl.handler       = other.impl.handler;
  other.impl.handler = nullptr;
  return *this;
  }
