#include "spatialscaler.h"

using namespace Tempest;

SpatialScaler::SpatialScaler(SpatialScaler&& other) noexcept
  :impl(std::move(other.impl)) {
  }

SpatialScaler::~SpatialScaler() {
  delete impl.handler;
  }

SpatialScaler& SpatialScaler::operator=(SpatialScaler&& other) noexcept {
  impl = std::move(other.impl);
  return *this;
  }
