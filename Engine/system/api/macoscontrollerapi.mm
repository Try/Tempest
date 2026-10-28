#include "macoscontrollerapi.h"

#include <Tempest/Platform>

#ifdef __OSX__

#import <AppKit/AppKit.h>
#import <GameController/GameController.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <stdexcept>

using namespace Tempest;
using namespace Tempest::Detail;

namespace {
using Button = GameController::Button;
using Axis = GameController::Axis;

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

GameController::State sample(GCExtendedGamepad* pad) {
  GameController::State state;
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

struct MacOSControllerApi::Impl {
  struct Device {
    GCController* controller;
    std::shared_ptr<ControllerState> input;
    };

  std::vector<Device> devices;
  NSMutableArray* observers = [[NSMutableArray alloc] init];
  bool active = NSApp.isActive;

  ~Impl() {
    assert([NSThread isMainThread]);
    for(id observer in observers)
      [[NSNotificationCenter defaultCenter] removeObserver:observer];
    [observers release];
    for(auto& device : devices) {
      clearHandlers(device.controller);
      device.input->disconnect();
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
    for(size_t i = 0; i < size_t(Axis::Count); ++i)
      device.input->axis(Axis(i), state.axes[i]);
    for(size_t i = 0; i < size_t(Button::Count); ++i) {
      const auto value = Button(i) == Button::LeftTrigger ? state.axes[size_t(Axis::LeftTrigger)] :
                         Button(i) == Button::RightTrigger ? state.axes[size_t(Axis::RightTrigger)] : 0.f;
      device.input->button(Button(i), (state.buttons & (uint32_t(1) << i)) != 0, value);
      }
    }

  void setActive(bool next) {
    assert([NSThread isMainThread]);
    if(next == active)
      return;
    active = next;
    for(const auto& device : devices) {
      // Refresh while inactive, then publish FocusGained after the fresh sample.
      // Window focus notifications can precede this native recovery notification.
      if(active)
        feedSample(device);
      device.input->setActive(active);
      }
    }

  void connect(GCController* controller) {
    assert([NSThread isMainThread]);
    if(controller.extendedGamepad == nil)
      return;
    for(const auto& device : devices)
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
    auto input = std::make_shared<ControllerState>(name == nullptr ? "Game controller" : name,
                                                  buttonMask, axisMask, sample(pad), active);
    devices.push_back({[controller retain], input});
    // Blocks copy weak_ptr itself, never a reference to a returned stack frame.
    std::weak_ptr<ControllerState> weak = input;
    for(size_t i = 0; i < b.size(); ++i) {
      b[i].valueChangedHandler = ^(GCControllerButtonInput*, float value, BOOL pressed) {
        if(auto state = weak.lock())
          state->button(Button(i), pressed, value);
        };
      }
    for(size_t i = 0; i < a.size(); ++i) {
      a[i].valueChangedHandler = ^(GCControllerAxisInput*, float value) {
        if(auto state = weak.lock())
          state->axis(Axis(i), value);
        };
      }
    }

  void disconnect(GCController* controller) {
    assert([NSThread isMainThread]);
    auto it = std::find_if(devices.begin(), devices.end(), [controller](const Device& device) {
      return device.controller == controller;
      });
    if(it == devices.end())
      return;
    clearHandlers(it->controller);
    it->input->disconnect();
    [it->controller release];
    devices.erase(it);
    }

  void observe(const std::shared_ptr<Impl>& self) {
    std::weak_ptr<Impl> weak = self;
    auto* center = [NSNotificationCenter defaultCenter];
    id connected = [center addObserverForName:GCControllerDidConnectNotification object:nil
      queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* notification) {
        if(auto self = weak.lock()) {
          auto* controller = static_cast<GCController*>(notification.object);
          if([[GCController controllers] containsObject:controller])
            self->connect(controller);
          }
        }];
    [observers addObject:connected];
    id disconnected = [center addObserverForName:GCControllerDidDisconnectNotification object:nil
      queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* notification) {
        if(auto self = weak.lock())
          self->disconnect(static_cast<GCController*>(notification.object));
        }];
    [observers addObject:disconnected];
    for(NSString* name in @[NSApplicationDidBecomeActiveNotification, NSApplicationWillResignActiveNotification]) {
      id observer = [center addObserverForName:name object:nil queue:[NSOperationQueue mainQueue]
        usingBlock:^(NSNotification* notification) {
          if(auto self = weak.lock())
            self->setActive([notification.name isEqualToString:NSApplicationDidBecomeActiveNotification]);
          }];
      [observers addObject:observer];
      }
    for(GCController* controller in [GCController controllers])
      connect(controller);
    }
  };

MacOSControllerApi::MacOSControllerApi() {
  requireMainThread();
  impl = std::make_shared<Impl>();
  impl->observe(impl);
  }

MacOSControllerApi::~MacOSControllerApi() = default;

void MacOSControllerApi::requireMainThread() {
  if(![NSThread isMainThread])
    throw std::logic_error("GameController::controllers must be called on the macOS main thread");
  }

std::vector<std::shared_ptr<ControllerState>> MacOSControllerApi::controllers() const {
  requireMainThread();
  std::vector<std::shared_ptr<ControllerState>> result;
  result.reserve(impl->devices.size());
  for(const auto& device : impl->devices)
    result.push_back(device.input);
  return result;
  }

#endif
