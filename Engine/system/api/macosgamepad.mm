#include "macosgamepad.h"

#include <Tempest/Platform>

#ifdef __OSX__

#include "../gamepadstate.h"

#import <AppKit/AppKit.h>
#import <GameController/GameController.h>

#include <algorithm>
#include <array>
#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

namespace {
using Button = Gamepad::Button;
using Axis = Gamepad::Axis;

std::array<GCControllerButtonInput*, size_t(Button::Count)> buttons(GCExtendedGamepad* pad) {
  return {{pad.buttonA, pad.buttonB, pad.buttonX, pad.buttonY,
           pad.dpad.up, pad.dpad.down, pad.dpad.left, pad.dpad.right,
           pad.leftShoulder, pad.rightShoulder, pad.leftThumbstickButton, pad.rightThumbstickButton,
           pad.leftTrigger, pad.rightTrigger, pad.buttonMenu, pad.buttonOptions, pad.buttonHome}};
  }

std::array<GCControllerAxisInput*, 4> stickAxes(GCExtendedGamepad* pad) {
  return {{pad.leftThumbstick.xAxis, pad.leftThumbstick.yAxis,
           pad.rightThumbstick.xAxis, pad.rightThumbstick.yAxis}};
  }

Gamepad::State sample(GCExtendedGamepad* pad) {
  Gamepad::State state;
  auto b = buttons(pad);
  for(size_t i = 0; i < b.size(); ++i)
    if(b[i].isPressed)
      state.buttons |= uint32_t(1) << i;
  auto a = stickAxes(pad);
  for(size_t i = 0; i < a.size(); ++i)
    state.axes[i] = a[i].value;
  state.axes[size_t(Axis::LeftTrigger)]  = pad.leftTrigger.value;
  state.axes[size_t(Axis::RightTrigger)] = pad.rightTrigger.value;
  return state;
  }
}

struct MacOSGamepad::Impl {
  struct Device {
    GCController* controller;
    Gamepad::DeviceId id;
    };

  GamepadState input;
  std::vector<Device> controllers;
  std::vector<void*> windows;
  NSMutableArray* observers = [[NSMutableArray alloc] init];
  bool active = false;
  bool appActive = NSApp.isActive;

  ~Impl() {
    assert([NSThread isMainThread]);
    for(id observer in observers)
      [[NSNotificationCenter defaultCenter] removeObserver:observer];
    [observers release];
    for(auto& device : controllers) {
      clearHandlers(device.controller);
      [device.controller release];
      }
    }

  static void clearHandlers(GCController* controller) {
    auto* pad = controller.extendedGamepad;
    for(auto* button : buttons(pad))
      button.valueChangedHandler = nil;
    for(auto* axis : stickAxes(pad))
      axis.valueChangedHandler = nil;
    }

  void feedSample(const Device& device) {
    const auto state = sample(device.controller.extendedGamepad);
    for(size_t i = 0; i < size_t(Button::Count); ++i)
      input.button(device.id, Button(i), (state.buttons & (uint32_t(1) << i)) != 0);
    for(size_t i = 0; i < size_t(Axis::Count); ++i)
      input.axis(device.id, Axis(i), state.axes[i]);
    }

  void updateFocus() {
    assert([NSThread isMainThread]);
    NSWindow* key = NSApp.keyWindow;
    const bool next = appActive && key != nil && key.isVisible && !key.isMiniaturized &&
                      std::find(windows.begin(), windows.end(), static_cast<void*>(key)) != windows.end();
    if(next == active)
      return;
    active = next;
    input.setActive(active);
    // Re-arm neutral controls immediately; held controls remain blocked.
    if(active)
      for(const auto& device : controllers)
        feedSample(device);
    }

  // Blocks must copy weak_ptr itself; a reference parameter would outlive its caller.
  void connect(GCController* controller, std::weak_ptr<Impl> weak) {
    assert([NSThread isMainThread]);
    if(controller.extendedGamepad == nil)
      return;
    for(const auto& device : controllers)
      if(device.controller == controller)
        return;
    controller.handlerQueue = dispatch_get_main_queue();
    auto* pad = controller.extendedGamepad;
    uint32_t buttonMask = 0;
    const auto b = buttons(pad);
    for(size_t i = 0; i < b.size(); ++i)
      if(b[i] != nil)
        buttonMask |= uint32_t(1) << i;
    uint32_t axisMask = 0;
    const auto a = stickAxes(pad);
    for(size_t i = 0; i < a.size(); ++i)
      if(a[i] != nil)
        axisMask |= uint32_t(1) << i;
    if(pad.leftTrigger != nil)
      axisMask |= uint32_t(1) << size_t(Axis::LeftTrigger);
    if(pad.rightTrigger != nil)
      axisMask |= uint32_t(1) << size_t(Axis::RightTrigger);
    const char* name = controller.vendorName.UTF8String;
    const auto id = input.connect(name == nullptr ? "Game controller" : name, buttonMask, axisMask, sample(pad));
    controllers.push_back({[controller retain], id});
    for(size_t i = 0; i < b.size(); ++i) {
      b[i].valueChangedHandler = ^(GCControllerButtonInput*, float value, BOOL pressed) {
        if(auto self = weak.lock()) {
          // Overflow is triggered by the edge: raw analog state must already
          // contain this sample so Reset also suppresses the held trigger axis.
          if(Button(i) == Button::LeftTrigger)
            self->input.axis(id, Axis::LeftTrigger, value);
          if(Button(i) == Button::RightTrigger)
            self->input.axis(id, Axis::RightTrigger, value);
          self->input.button(id, Button(i), pressed);
          }
        };
      }
    for(size_t i = 0; i < a.size(); ++i) {
      a[i].valueChangedHandler = ^(GCControllerAxisInput*, float value) {
        if(auto self = weak.lock())
          self->input.axis(id, Axis(i), value);
        };
      }
    }

  void disconnect(GCController* controller) {
    assert([NSThread isMainThread]);
    auto it = std::find_if(controllers.begin(), controllers.end(), [controller](const Device& device) {
      return device.controller == controller;
      });
    if(it == controllers.end())
      return;
    clearHandlers(it->controller);
    input.disconnect(it->id);
    [it->controller release];
    controllers.erase(it);
    }

  void observe(const std::shared_ptr<Impl>& self) {
    std::weak_ptr<Impl> weak = self;
    auto* center = [NSNotificationCenter defaultCenter];
    id connected = [center addObserverForName:GCControllerDidConnectNotification object:nil
      queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* notification) {
        if(auto self = weak.lock()) {
          auto* controller = static_cast<GCController*>(notification.object);
          if([[GCController controllers] containsObject:controller])
            self->connect(controller, weak);
          }
        }];
    [observers addObject:connected];
    id disconnected = [center addObserverForName:GCControllerDidDisconnectNotification object:nil
      queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* notification) {
        if(auto self = weak.lock())
          self->disconnect(static_cast<GCController*>(notification.object));
        }];
    [observers addObject:disconnected];
    for(NSString* name in @[NSApplicationDidBecomeActiveNotification, NSApplicationWillResignActiveNotification,
                            NSWindowDidBecomeKeyNotification, NSWindowDidResignKeyNotification,
                            NSWindowDidMiniaturizeNotification, NSWindowDidDeminiaturizeNotification,
                            NSWindowWillCloseNotification]) {
      id observer = [center addObserverForName:name object:nil queue:[NSOperationQueue mainQueue]
        usingBlock:^(NSNotification* notification) {
          if(auto self = weak.lock()) {
            if([notification.name isEqualToString:NSWindowWillCloseNotification]) {
              auto window = static_cast<void*>(notification.object);
              std::erase(self->windows, window);
              }
            if([notification.name isEqualToString:NSApplicationWillResignActiveNotification])
              self->appActive = false;
            if([notification.name isEqualToString:NSApplicationDidBecomeActiveNotification])
              self->appActive = true;
            self->updateFocus();
            }
          }];
      [observers addObject:observer];
      }
    for(GCController* controller in [GCController controllers])
      connect(controller, weak);
    }
  };

MacOSGamepad::MacOSGamepad() : impl(std::make_shared<Impl>()) {
  assert([NSThread isMainThread]);
  impl->observe(impl);
  }

MacOSGamepad::~MacOSGamepad() = default;

void MacOSGamepad::addWindow(void* window) {
  assert([NSThread isMainThread]);
  impl->windows.push_back(window);
  impl->updateFocus();
  }

void MacOSGamepad::removeWindow(void* window) {
  assert([NSThread isMainThread]);
  std::erase(impl->windows, window);
  impl->updateFocus();
  }

std::vector<Gamepad::Device> MacOSGamepad::devices() const {
  assert([NSThread isMainThread]);
  return impl->input.devices();
  }

bool MacOSGamepad::state(Gamepad::DeviceId id, Gamepad::State& out) const {
  assert([NSThread isMainThread]);
  return impl->input.state(id, out);
  }

bool MacOSGamepad::pollEvent(Gamepad::Event& out) {
  assert([NSThread isMainThread]);
  return impl->input.pollEvent(out);
  }

#endif
