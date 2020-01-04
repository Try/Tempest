#pragma once

#include <Tempest/HeadlessDevice>

namespace Tempest {

class Device : public HeadlessDevice {
  public:
    using Caps=AbstractGraphicsApi::Caps;

    Device(AbstractGraphicsApi& api, uint8_t maxFramesInFlight=2);
    Device(const Device&)=delete;
    virtual ~Device();

    uint8_t maxFramesInFlight() const;

  private:
    struct Impl {
      Impl(uint8_t maxFramesInFlight);
      ~Impl();

      uint8_t maxFramesInFlight=1;
      };

    Impl impl;
  };
}
