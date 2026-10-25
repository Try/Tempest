#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Tempest {

// Main-thread API with one event consumer per application. Create a Window first.
class Gamepad final {
  public:
    using DeviceId = uint64_t;

    enum class Button : uint8_t {
      South, East, West, North,
      DpadUp, DpadDown, DpadLeft, DpadRight,
      LeftShoulder, RightShoulder, LeftThumb, RightThumb,
      LeftTrigger, RightTrigger, Menu, View, Home,
      Count
      };

    enum class Axis : uint8_t {
      LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger, Count
      };

    struct Device {
      DeviceId    id = 0;
      std::string name;
      // Bit N describes Button(N) or Axis(N). Missing optional controls stay zero.
      uint32_t    supportedButtons = 0;
      uint32_t    supportedAxes = 0;
      };

    struct State {
      uint32_t buttons = 0;
      // Sticks: [-1,1], positive Y is up. Triggers: [0,1]. No game dead zone.
      std::array<float, size_t(Axis::Count)> axes = {};
      };

    struct Event {
      enum class Type : uint8_t { Connected, Disconnected, ButtonDown, ButtonUp, Reset };
      enum class ResetReason : uint8_t { None, FocusLost, Disconnected, Overflow };

      Type        type = Type::Reset;
      DeviceId    device = 0;
      Button      button = Button::South; // Only meaningful for button events.
      ResetReason reason = ResetReason::None;
      };

    // Connection IDs are not reused. Re-enumerate after a global Reset.
    // Other platforms currently return no devices/events.
    static std::vector<Device> devices();
    // Unknown/disconnected IDs return false and clear out.
    static bool state(DeviceId id, State& out);
    // Drain regularly, even while paused. The queue preserves up to 1024 edges.
    // Focus loss/disconnect cancel pending input and clear state. Held controls
    // must return to neutral before reuse after a reset or connection.
    // Reset cancels held actions; it is not a physical release. An Overflow
    // Reset has device=0 and requires re-enumerating devices and cancelling all.
    static bool pollEvent(Event& out);
  };

}
