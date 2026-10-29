#pragma once

#include <Tempest/Vec>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Tempest {

class ControllerApi;
namespace Detail {
class MacosController;
}

class GameController final {
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

    enum class Stick : uint8_t { Left, Right };

    struct Device {
      DeviceId    id = 0;
      std::string name;
      // Bit N describes Button(N) or Axis(N). Missing controls stay zero.
      uint32_t    supportedButtons = 0;
      uint32_t    supportedAxes = 0;
      };

    struct State {
      uint32_t buttons = 0;
      // Sticks: [-1,1], positive Y up. Triggers: [0,1]. Native filtering applies.
      std::array<float, size_t(Axis::Count)> axes = {};
      };

    struct Event {
      enum class Type : uint8_t { Connected, Disconnected, ButtonDown, ButtonUp, Reset };
      enum class ResetReason : uint8_t { None, FocusLost, Disconnected, Overflow, FocusGained };

      Type        type = Type::Reset;
      DeviceId    device = 0;
      Button      button = Button::South; // Only meaningful for button events.
      ResetReason reason = ResetReason::None;
      };

    const Device& device() const;
    State state() const;
    Vec2  axis(Stick stick) const;

    // Caller-thread callbacks; serialize pulls across copies without reentry.
    // See docs/game-controller.md for cancellation, capacity and lifetime rules.
    void pull(const std::function<void(const Event&)>& callback) const;

  private:
    struct Impl;
    explicit GameController(std::shared_ptr<Impl> impl);
    std::shared_ptr<Impl> impl;

  friend class ControllerApi;
  friend class Detail::MacosController;
  };

}
