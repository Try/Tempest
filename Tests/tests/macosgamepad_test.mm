// Standalone backend regression target: do not also compile macosgamepad.mm.
// No NSApplication or window is needed; synthetic callbacks exercise the adapter.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#define private public
#include "../../Engine/system/api/macosgamepad.h"
#undef private
#include "../../Engine/system/api/macosgamepad.mm"

using Tempest::Gamepad;
using Tempest::Detail::MacOSGamepad;

namespace {
void require(bool condition, const char* message) {
  if(!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
    }
  }

// This scope must end before handlers execute: capturing this weak_ptr reference
// previously left every installed input block pointing into a returned stack.
__attribute__((noinline))
void connectWithLocalWeak(MacOSGamepad& backend, GCController* controller) {
  std::weak_ptr<MacOSGamepad::Impl> weak = backend.impl;
  backend.impl->connect(controller, weak);
  }

void expectEvent(MacOSGamepad& backend, Gamepad::Event::Type type,
                 Gamepad::DeviceId id, Gamepad::Button button = Gamepad::Button::South) {
  Gamepad::Event event;
  require(backend.pollEvent(event), "expected event missing");
  require(event.type == type && event.device == id, "event type/device mismatch");
  if(type == Gamepad::Event::Type::ButtonDown || type == Gamepad::Event::Type::ButtonUp)
    require(event.button == button, "button order mismatch");
  }
}

int main() {
  @autoreleasepool {
    auto backend = std::make_unique<MacOSGamepad>();
    // Isolate this process's storage from real controllers enumerated at startup;
    // this removes only our handlers/state and never disconnects physical devices.
    const auto existing = backend->impl->controllers;
    for(const auto& device : existing)
      backend->impl->disconnect(device.controller);
    Gamepad::Event startupEvent;
    while(backend->pollEvent(startupEvent)) {}
    backend->impl->input.setActive(true);
    GCController* controller = [GCController controllerWithExtendedGamepad];
    connectWithLocalWeak(*backend, controller);
    require(backend->devices().size() == 1, "snapshot did not connect");
    const auto id = backend->devices().front().id;
    expectEvent(*backend, Gamepad::Event::Type::Connected, id);

    // Invoke the actual installed handlers after connectWithLocalWeak returned.
    // The arguments intentionally differ from the snapshot's unchanged state,
    // verifying that short edges use callback arguments rather than late reads.
    const auto inputs = buttons(controller.extendedGamepad);
    for(size_t i = 0; i < inputs.size(); ++i) {
      if(inputs[i] == nil)
        continue;
      auto handler = inputs[i].valueChangedHandler;
      require(handler != nil, "button handler missing");
      handler(inputs[i], 1.f, YES);
      handler(inputs[i], 0.f, NO);
      expectEvent(*backend, Gamepad::Event::Type::ButtonDown, id, Gamepad::Button(i));
      expectEvent(*backend, Gamepad::Event::Type::ButtonUp, id, Gamepad::Button(i));
      }
    const auto axes = stickAxes(controller.extendedGamepad);
    for(size_t i = 0; i < axes.size(); ++i) {
      require(axes[i].valueChangedHandler != nil, "axis handler missing");
      axes[i].valueChangedHandler(axes[i], i % 2 == 0 ? 0.75f : -0.5f);
      }
    controller.extendedGamepad.leftTrigger.valueChangedHandler(controller.extendedGamepad.leftTrigger, 0.25f, NO);
    controller.extendedGamepad.rightTrigger.valueChangedHandler(controller.extendedGamepad.rightTrigger, 0.8f, NO);
    Gamepad::State state;
    require(backend->state(id, state), "connected state missing");
    require(state.buttons == 0, "short press left a held button");
    require(state.axes == std::array<float, 6>{{0.75f, -0.5f, 0.75f, -0.5f, 0.25f, 0.8f}},
            "axis/trigger callback arguments lost");
    Gamepad::Event event;
    require(!backend->pollEvent(event), "unexpected duplicate button event");

    // A trigger callback updates both analog and pressed state. If its edge
    // overflows the queue, Reset must cancel the analog value in the same sample.
    for(auto* axis : axes)
      axis.valueChangedHandler(axis, 0.f);
    for(auto* trigger : {controller.extendedGamepad.leftTrigger, controller.extendedGamepad.rightTrigger})
      trigger.valueChangedHandler(trigger, 0.f, NO);
    for(size_t i = 0; i < 2; ++i) {
      auto* trigger = i == 0 ? controller.extendedGamepad.leftTrigger : controller.extendedGamepad.rightTrigger;
      const auto triggerButton = i == 0 ? Gamepad::Button::LeftTrigger : Gamepad::Button::RightTrigger;
      const auto triggerAxis = i == 0 ? Gamepad::Axis::LeftTrigger : Gamepad::Axis::RightTrigger;
      auto* south = controller.extendedGamepad.buttonA;
      for(size_t edge = 0; edge < Tempest::Detail::GamepadState::MaxEvents; ++edge)
        south.valueChangedHandler(south, edge % 2 == 0 ? 1.f : 0.f, edge % 2 == 0);
      trigger.valueChangedHandler(trigger, 1.f, YES);
      require(backend->pollEvent(event) && event.type == Gamepad::Event::Type::Reset &&
              event.device == 0 && event.reason == Gamepad::Event::ResetReason::Overflow,
              "trigger edge did not produce a global Overflow Reset");
      require(!backend->pollEvent(event), "overflow left stale button events");
      require(backend->state(id, state) && state.buttons == 0 && state.axes == Gamepad::State{}.axes,
              "overflow leaked its triggering analog sample");
      trigger.valueChangedHandler(trigger, 0.9f, YES);
      require(backend->state(id, state) && state.buttons == 0 && state.axes == Gamepad::State{}.axes,
              "held trigger escaped suppression after overflow");
      trigger.valueChangedHandler(trigger, 0.f, NO);
      require(!backend->pollEvent(event), "suppressed trigger emitted a release edge");
      trigger.valueChangedHandler(trigger, 0.65f, YES);
      expectEvent(*backend, Gamepad::Event::Type::ButtonDown, id, triggerButton);
      require(backend->state(id, state) && state.axes[size_t(triggerAxis)] == 0.65f &&
              state.buttons == (uint32_t(1) << uint32_t(triggerButton)),
              "trigger failed to resume after neutral");
      trigger.valueChangedHandler(trigger, 0.f, NO);
      expectEvent(*backend, Gamepad::Event::Type::ButtonUp, id, triggerButton);
      }

    // GameController may already have copied a callback for queued delivery.
    auto oldButton = Block_copy(controller.extendedGamepad.buttonA.valueChangedHandler);
    auto oldAxis = Block_copy(controller.extendedGamepad.leftThumbstick.xAxis.valueChangedHandler);
    backend->impl->disconnect(controller);
    expectEvent(*backend, Gamepad::Event::Type::Reset, id);
    expectEvent(*backend, Gamepad::Event::Type::Disconnected, id);
    oldButton(controller.extendedGamepad.buttonA, 1.f, YES);
    oldAxis(controller.extendedGamepad.leftThumbstick.xAxis, 1.f);
    require(!backend->state(id, state) && state.buttons == 0 && state.axes == Gamepad::State{}.axes,
            "stale disconnected state was recreated");
    require(!backend->pollEvent(event), "late disconnected callback emitted an event");

    connectWithLocalWeak(*backend, controller);
    const auto reconnected = backend->devices().front().id;
    require(reconnected != id, "reconnect reused its old ID");
    expectEvent(*backend, Gamepad::Event::Type::Connected, reconnected);
    auto liveButton = Block_copy(controller.extendedGamepad.buttonA.valueChangedHandler);
    auto liveAxis = Block_copy(controller.extendedGamepad.leftThumbstick.xAxis.valueChangedHandler);
    std::weak_ptr<MacOSGamepad::Impl> lifetime = backend->impl;
    backend.reset();
    require(lifetime.expired(), "handlers retained the backend");
    require(controller.extendedGamepad.buttonA.valueChangedHandler == nil &&
            controller.extendedGamepad.leftThumbstick.xAxis.valueChangedHandler == nil,
            "destruction left installed handlers");
    oldButton(controller.extendedGamepad.buttonA, 0.f, NO);
    oldAxis(controller.extendedGamepad.leftThumbstick.xAxis, 0.f);
    liveButton(controller.extendedGamepad.buttonA, 1.f, YES);
    liveAxis(controller.extendedGamepad.leftThumbstick.xAxis, 1.f);
    Block_release(oldButton);
    Block_release(oldAxis);
    Block_release(liveButton);
    Block_release(liveAxis);
    std::puts("PASS: macOS callback arguments, trigger overflow, scope lifetime, disconnect/reconnect, destruction");
    }
  }
