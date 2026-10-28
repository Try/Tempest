#pragma once

#include "gamecontroller.h"

#include <deque>
#include <mutex>

namespace Tempest::Detail {

// A connection's portable state; public handles never own native UI objects.
class ControllerState final {
  public:
    using Button = GameController::Button;
    using Axis   = GameController::Axis;
    using State  = GameController::State;
    using Device = GameController::Device;
    using Event  = GameController::Event;

    static constexpr size_t MaxEvents = 1024;

    ControllerState(std::string name, uint32_t buttons, uint32_t axes, const State& initial, bool active = true);

    const Device& device() const { return info; }
    State state() const;
    void pull(const std::function<void(const Event&)>& callback);

    void disconnect();
    void setActive(bool active);
    // Button callbacks also carry the analog value for trigger buttons.
    void button(Button button, bool pressed, float value);
    void axis(Axis axis, float value);

  private:
    void push(const Event& event);

    const Device       info;
    mutable std::mutex sync;
    State              current;
    std::deque<Event>  events;
    bool               connected = true;
    bool               active;
    bool               pulling = false;
  };

}
