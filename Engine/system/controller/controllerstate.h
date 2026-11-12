#pragma once

#include "../gamecontroller.h"

#include <deque>
#include <mutex>

namespace Tempest {

struct GameController::Impl {
  public:
    virtual ~Impl() = default;

    const Device& device() const { return info; }
    State state() const;
    void pull(const std::function<void(const Event&)>& callback);

  protected:
    static constexpr size_t MaxEvents = 1024;

    Impl(std::string name, uint32_t buttons, uint32_t axes, const State& initial, bool active = true);

    void disconnect();
    void setActive(bool active);
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
  };

}
