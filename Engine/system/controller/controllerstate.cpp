#include "controllerstate.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <utility>

using namespace Tempest;

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

GameController::Impl::Impl(std::string name, uint32_t buttons, uint32_t axes,
                           const State& initial, bool active)
  : info{connectionId(), std::move(name), buttons & buttonMask, axes & axisMask}, active(active) {
  current.buttons = initial.buttons & info.supportedButtons;
  for(size_t i = 0; i < initial.axes.size(); ++i)
    if((info.supportedAxes & (uint32_t(1) << i)) != 0 && std::isfinite(initial.axes[i]))
      current.axes[i] = normalized(Axis(i), initial.axes[i]);
  push({Event::Type::Connected, info.id});
  }

GameController::State GameController::Impl::state() const {
  std::lock_guard<std::mutex> guard(sync);
  return current;
  }

void GameController::Impl::pull(const std::function<void(const Event&)>& callback) {
  size_t count;
  {
    std::lock_guard<std::mutex> guard(sync);
    count = events.size();
    }
  // New input must not keep a game tick inside pull indefinitely.
  for(size_t i = 0; i < count; ++i) {
    Event event;
    {
      std::lock_guard<std::mutex> guard(sync);
      if(events.empty())
        break;
      event = events.front();
      events.pop_front();
      }
    callback(event);
    }
  }

void GameController::Impl::disconnect() {
  std::lock_guard<std::mutex> guard(sync);
  if(!connected)
    return;
  connected = false;
  current = {};
  events.clear();
  push({Event::Type::Reset, info.id, Button::South, Event::ResetReason::Disconnected});
  push({Event::Type::Disconnected, info.id});
  }

void GameController::Impl::setActive(bool value) {
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

void GameController::Impl::button(Button button, bool pressed, float value) {
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

void GameController::Impl::axis(Axis axis, float value) {
  if(uint32_t(axis) >= uint32_t(Axis::Count) || !std::isfinite(value))
    return;
  std::lock_guard<std::mutex> guard(sync);
  if(connected && (info.supportedAxes & (uint32_t(1) << uint32_t(axis))) != 0)
    current.axes[size_t(axis)] = normalized(axis, value);
  }

void GameController::Impl::push(const Event& event) {
  if(events.size() < MaxEvents) {
    events.push_back(event);
    return;
    }
  events.clear();
  // The current native snapshot survives loss of history. Rearming is game policy.
  events.push_back({Event::Type::Reset, info.id, Button::South, Event::ResetReason::Overflow});
  }
