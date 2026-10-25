#include "gamepad.h"

#include "systemapi.h"

using namespace Tempest;

std::vector<Gamepad::Device> Gamepad::devices() {
  return SystemApi::inst().implGamepadDevices();
  }

bool Gamepad::state(DeviceId id, State& out) {
  return SystemApi::inst().implGamepadState(id, out);
  }

bool Gamepad::pollEvent(Event& out) {
  return SystemApi::inst().implPollGamepadEvent(out);
  }
