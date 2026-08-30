#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
template<class T>
class Encoder;

class TemporalScaler final {
  public:
    TemporalScaler() = default;
    TemporalScaler(TemporalScaler&& other) noexcept;
    TemporalScaler(const TemporalScaler&) = delete;
    ~TemporalScaler();

    TemporalScaler& operator=(TemporalScaler&& other) noexcept;
    TemporalScaler& operator=(const TemporalScaler&) = delete;

    bool isEmpty() const { return impl.handler==nullptr; }
    explicit operator bool() const { return !isEmpty(); }

  private:
    explicit TemporalScaler(AbstractGraphicsApi::TemporalScaler* scaler):impl(scaler) {}

    Detail::DPtr<AbstractGraphicsApi::TemporalScaler*> impl;

  friend class Tempest::Device;
  friend class Encoder<Tempest::CommandBuffer>;
  };

}
