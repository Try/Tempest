#include "gamepadstate.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

using namespace Tempest;
using namespace Tempest::Detail;

namespace {
constexpr uint32_t buttonMask = (uint32_t(1) << uint32_t(Gamepad::Button::Count)) - 1;
constexpr uint32_t axisMask = (uint32_t(1) << uint32_t(Gamepad::Axis::Count)) - 1;
constexpr float rearmThreshold = 0.1f;

float normalized(Gamepad::Axis axis, float value) {
  const bool trigger = axis == Gamepad::Axis::LeftTrigger || axis == Gamepad::Axis::RightTrigger;
  return std::max(trigger ? 0.f : -1.f, std::min(1.f, value));
  }

bool isButtonEvent(const Gamepad::Event& event) {
  return event.type == Gamepad::Event::Type::ButtonDown ||
         event.type == Gamepad::Event::Type::ButtonUp;
  }
}

GamepadState::DeviceId GamepadState::connect(std::string name, uint32_t buttons,
                                           uint32_t axes, const State& initial) {
  if(nextId == 0)
    throw std::overflow_error("Gamepad connection ID exhausted");
  Entry entry;
  entry.device = {nextId++, std::move(name), buttons & buttonMask, axes & axisMask};
  entry.raw.buttons = initial.buttons & entry.device.supportedButtons;
  entry.blockedButtons = active ? entry.raw.buttons : entry.device.supportedButtons;
  for(size_t i = 0; i < initial.axes.size(); ++i) {
    entry.raw.axes[i] = std::isfinite(initial.axes[i]) ? normalized(Axis(i), initial.axes[i])
                                                     : initial.axes[i];
    if(!active || !std::isfinite(entry.raw.axes[i]) || std::abs(entry.raw.axes[i]) > rearmThreshold)
      entry.blockedAxes |= (uint32_t(1) << i) & entry.device.supportedAxes;
    }
  const auto id = entry.device.id;
  entries.push_back(std::move(entry));
  push({Event::Type::Connected, id});
  return id;
  }

void GamepadState::disconnect(DeviceId id) {
  auto* entry = find(id);
  if(entry == nullptr)
    return;
  cancel(*entry, Event::ResetReason::Disconnected);
  entries.erase(std::remove_if(entries.begin(), entries.end(), [id](const Entry& e) {
    return e.device.id == id;
    }), entries.end());
  push({Event::Type::Disconnected, id});
  }

void GamepadState::setActive(bool value) {
  if(active == value)
    return;
  active = value;
  for(auto& entry: entries) {
    if(!active) {
      cancel(entry, Event::ResetReason::FocusLost);
      } else {
      // The backend supplies a full current sample after focus returns.
      entry.blockedButtons = entry.device.supportedButtons;
      entry.blockedAxes = entry.device.supportedAxes;
      }
    }
  }

void GamepadState::button(DeviceId id, Button button, bool pressed) {
  if(uint32_t(button) >= uint32_t(Button::Count))
    return;
  auto* entry = find(id);
  const auto bit = uint32_t(1) << uint32_t(button);
  if(entry == nullptr || (entry->device.supportedButtons & bit) == 0)
    return;
  if(pressed)
    entry->raw.buttons |= bit;
  else
    entry->raw.buttons &= ~bit;
  if(!active)
    return;
  if((entry->blockedButtons & bit) != 0) {
    if(!pressed)
      entry->blockedButtons &= ~bit;
    return;
    }
  if(((entry->state.buttons & bit) != 0) == pressed)
    return;
  // push may cancel every state on overflow; never publish that triggering edge.
  if(!push({pressed ? Event::Type::ButtonDown : Event::Type::ButtonUp, id, button}))
    return;
  if(pressed)
    entry->state.buttons |= bit;
  else
    entry->state.buttons &= ~bit;
  }

void GamepadState::axis(DeviceId id, Axis axis, float value) {
  if(uint32_t(axis) >= uint32_t(Axis::Count) || !std::isfinite(value))
    return;
  auto* entry = find(id);
  const auto bit = uint32_t(1) << uint32_t(axis);
  if(entry == nullptr || (entry->device.supportedAxes & bit) == 0)
    return;
  value = normalized(axis, value);
  entry->raw.axes[size_t(axis)] = value;
  if(!active)
    return;
  if((entry->blockedAxes & bit) != 0) {
    if(std::abs(value) <= rearmThreshold)
      entry->blockedAxes &= ~bit;
    return;
    }
  entry->state.axes[size_t(axis)] = value;
  }

std::vector<GamepadState::Device> GamepadState::devices() const {
  std::vector<Device> result;
  result.reserve(entries.size());
  for(const auto& entry: entries)
    result.push_back(entry.device);
  return result;
  }

bool GamepadState::state(DeviceId id, State& out) const {
  for(const auto& entry: entries) {
    if(entry.device.id == id) {
      out = entry.state;
      return true;
      }
    }
  out = {};
  return false;
  }

bool GamepadState::pollEvent(Event& out) {
  if(events.empty())
    return false;
  out = events.front();
  events.pop_front();
  return true;
  }

GamepadState::Entry* GamepadState::find(DeviceId id) {
  for(auto& entry: entries)
    if(entry.device.id == id)
      return &entry;
  return nullptr;
  }

bool GamepadState::push(const Event& event) {
  if(events.size() < MaxEvents) {
    events.push_back(event);
    return true;
    }
  events.clear();
  for(auto& entry: entries) {
    entry.state = {};
    entry.blockedButtons = entry.raw.buttons;
    entry.blockedAxes = 0;
    for(size_t i = 0; i < entry.raw.axes.size(); ++i) {
      if(!std::isfinite(entry.raw.axes[i]) || std::abs(entry.raw.axes[i]) > rearmThreshold)
        entry.blockedAxes |= (uint32_t(1) << i) & entry.device.supportedAxes;
      }
    }
  events.push_back({Event::Type::Reset, 0, Button::South, Event::ResetReason::Overflow});
  return false;
  }

void GamepadState::cancel(Entry& entry, Event::ResetReason reason) {
  const auto id = entry.device.id;
  events.erase(std::remove_if(events.begin(), events.end(), [id](const Event& event) {
    return event.device == id && isButtonEvent(event);
    }), events.end());
  entry.state = {};
  entry.blockedButtons = entry.device.supportedButtons;
  entry.blockedAxes = entry.device.supportedAxes;
  push({Event::Type::Reset, id, Button::South, reason});
  }
