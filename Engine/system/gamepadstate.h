#pragma once

#include "gamepad.h"

#include <deque>

namespace Tempest::Detail {

// Platform-neutral main-thread storage. The backend feeds each callback's value.
class GamepadState final {
  public:
    using DeviceId = Gamepad::DeviceId;
    using Button   = Gamepad::Button;
    using Axis     = Gamepad::Axis;
    using State    = Gamepad::State;
    using Device   = Gamepad::Device;
    using Event    = Gamepad::Event;

    static constexpr size_t MaxEvents = 1024;

    DeviceId connect(std::string name, uint32_t buttons, uint32_t axes, const State& initial);
    void     disconnect(DeviceId id);
    void     setActive(bool active);
    void     button(DeviceId id, Button button, bool pressed);
    void     axis(DeviceId id, Axis axis, float value);

    std::vector<Device> devices() const;
    bool state(DeviceId id, State& out) const;
    bool pollEvent(Event& out);

  private:
    // Internal representation is owned by the portable-state implementer.
    struct Entry {
      Device   device;
      State    state;
      // Last physical sample survives cancellation for overflow rearming.
      State    raw;
      uint32_t blockedButtons = 0;
      uint32_t blockedAxes = 0;
      };

    Entry* find(DeviceId id);
    bool   push(const Event& event);
    void   cancel(Entry& entry, Event::ResetReason reason);

    std::vector<Entry> entries;
    std::deque<Event>  events;
    DeviceId          nextId = 1;
    bool              active = false;
  };

}
