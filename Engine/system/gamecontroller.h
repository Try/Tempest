#pragma once

#include <Tempest/Vec>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Tempest {

namespace Detail {
class ControllerApi;
class ControllerState;
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

    // macOS: call on the main thread, after NSApplication finishes launching,
    // and keep its main run loop running. No Tempest Window is required.
    // Copies and repeated enumeration share one connection and event queue.
    // Other platforms currently return no controllers.
    static std::vector<GameController> controllers();

    const Device& device() const;
    // Thread-safe latest native sample, including while inactive. Disconnect
    // clears the sample. No game dead zone or neutral rearming is applied.
    State state() const;
    Vec2  axis(Stick stick) const;

    // Synchronous callbacks on the caller's thread, with no internal lock held.
    // One serialized consumer per connection, shared by every copy. Recursive
    // or overlapping pulls throw std::logic_error before consuming an event.
    // A throwing callback consumes that event; untouched events remain queued.
    // Each pull delivers at most the queue length observed at entry.
    // Up to 1024 observed edges are retained. Reset cancels pending input for
    // this connection; inspect state() and cancel held actions in the game.
    // Disconnect is terminal and remains observable on stale handles.
    void pull(const std::function<void(const Event&)>& callback) const;

  private:
    explicit GameController(std::shared_ptr<Detail::ControllerState> impl);
    std::shared_ptr<Detail::ControllerState> impl;

  friend class Detail::ControllerApi;
  };

}
