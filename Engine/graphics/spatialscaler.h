#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
template<class T>
class Encoder;

class SpatialScaler final {
  public:
    SpatialScaler() = default;
    SpatialScaler(SpatialScaler&& other) noexcept;
    ~SpatialScaler();

    SpatialScaler& operator=(SpatialScaler&& other) noexcept;

    bool isEmpty() const { return impl.handler==nullptr; }

  private:
    explicit SpatialScaler(AbstractGraphicsApi::SpatialScaler* scaler):impl(scaler) {}

    Detail::DPtr<AbstractGraphicsApi::SpatialScaler*> impl;

  friend class Tempest::Device;
  friend class Encoder<Tempest::CommandBuffer>;
  };

}
