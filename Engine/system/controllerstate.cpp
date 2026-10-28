#include "controllerstate.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <utility>

using namespace Tempest;
using namespace Tempest::Detail;

namespace {
constexpr uint32_t buttonMask = (uint32_t(1) << uint32_t(GameController::Button::Count)) - 1;
constexpr uint32_t axisMask   = (uint32_t(1) << uint32_t(GameController::Axis::Count)) - 1;

GameController::DeviceId connectionId() {
  static std::atomic<GameController::DeviceId> next{1};
  auto id = next.load();
  do {
    if(id == 0)
      throw std::overflow_error("GameController connection ID exhausted");
    } while(!next.compare_exchange_weak(id, id + 1));
  return id;
  }

float normalized(GameController::Axis axis, float value) {
  const bool trigger = axis == GameController::Axis::LeftTrigger || axis == GameController::Axis::RightTrigger;
  return std::clamp(value, trigger ? 0.f : -1.f, 1.f);
  }
}

ControllerState::ControllerState(std::string name, uint32_t buttons, uint32_t axes,
                                 const State& initial, bool active)
  : info{connectionId(), std::move(name), buttons & buttonMask, axes & axisMask}, active(active) {
  current.buttons = initial.buttons & info.supportedButtons;
  for(size_t i = 0; i < initial.axes.size(); ++i)
    if((info.supportedAxes & (uint32_t(1) << i)) != 0 && std::isfinite(initial.axes[i]))
      current.axes[i] = normalized(Axis(i), initial.axes[i]);
  push({Event::Type::Connected, info.id});
  }

ControllerState::State ControllerState::state() const {
  std::lock_guard<std::mutex> guard(sync);
  return current;
  }

void ControllerState::pull(const std::function<void(const Event&)>& callback) {
  std::unique_lock<std::mutex> guard(sync);
  if(pulling)
    throw std::logic_error("GameController::pull already in progress for this connection");
  pulling = true;
  try {
    // New input must not keep a game tick inside pull indefinitely.
    const auto count = events.size();
    for(size_t i = 0; i < count && !events.empty(); ++i) {
      const auto event = events.front();
      events.pop_front();
      guard.unlock();
      callback(event);
      guard.lock();
      }
    } catch(...) {
    if(!guard.owns_lock())
      guard.lock();
    pulling = false;
    throw;
    }
  pulling = false;
  }

void ControllerState::disconnect() {
  std::lock_guard<std::mutex> guard(sync);
  if(!connected)
    return;
  connected = false;
  current = {};
  events.clear();
  push({Event::Type::Reset, info.id, Button::South, Event::ResetReason::Disconnected});
  push({Event::Type::Disconnected, info.id});
  }

void ControllerState::setActive(bool value) {
  std::lock_guard<std::mutex> guard(sync);
  if(!connected || active == value)
    return;
  active = value;
  if(!active) {
    events.clear();
    push({Event::Type::Reset, info.id, Button::South, Event::ResetReason::FocusLost});
    } else {
    // Native focus recovery refreshes the sample before publishing this barrier.
    push({Event::Type::Reset, info.id, Button::South, Event::ResetReason::FocusGained});
    }
  }

void ControllerState::button(Button button, bool pressed, float value) {
  if(uint32_t(button) >= uint32_t(Button::Count))
    return;
  std::lock_guard<std::mutex> guard(sync);
  const auto bit = uint32_t(1) << uint32_t(button);
  if(!connected || (info.supportedButtons & bit) == 0)
    return;
  if(button == Button::LeftTrigger || button == Button::RightTrigger) {
    const auto axis = button == Button::LeftTrigger ? Axis::LeftTrigger : Axis::RightTrigger;
    if((info.supportedAxes & (uint32_t(1) << uint32_t(axis))) != 0 && std::isfinite(value))
      current.axes[size_t(axis)] = normalized(axis, value);
    }
  if(((current.buttons & bit) != 0) == pressed)
    return;
  if(pressed)
    current.buttons |= bit;
  else
    current.buttons &= ~bit;
  if(active)
    push({pressed ? Event::Type::ButtonDown : Event::Type::ButtonUp, info.id, button});
  }

void ControllerState::axis(Axis axis, float value) {
  if(uint32_t(axis) >= uint32_t(Axis::Count) || !std::isfinite(value))
    return;
  std::lock_guard<std::mutex> guard(sync);
  if(connected && (info.supportedAxes & (uint32_t(1) << uint32_t(axis))) != 0)
    current.axes[size_t(axis)] = normalized(axis, value);
  }

void ControllerState::push(const Event& event) {
  if(events.size() < MaxEvents) {
    events.push_back(event);
    return;
    }
  events.clear();
  // The current native snapshot survives loss of history. Rearming is game policy.
  events.push_back({Event::Type::Reset, info.id, Button::South, Event::ResetReason::Overflow});
  }
