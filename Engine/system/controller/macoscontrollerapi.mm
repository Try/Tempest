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

namespace {
uint32_t supportedButtons(GCController* controller) {
  uint32_t mask = 0;
  const auto values = buttons(controller.extendedGamepad);
  for(size_t i = 0; i < values.size(); ++i)
    if(values[i] != nil)
      mask |= uint32_t(1) << i;
  return mask;
  }

uint32_t supportedAxes(GCController* controller) {
  uint32_t mask = 0;
  auto* pad = controller.extendedGamepad;
  const auto values = stickAxes(pad);
  for(size_t i = 0; i < values.size(); ++i)
    if(values[i] != nil)
      mask |= uint32_t(1) << i;
  if(pad.leftTrigger != nil)
    mask |= uint32_t(1) << size_t(Axis::LeftTrigger);
  if(pad.rightTrigger != nil)
    mask |= uint32_t(1) << size_t(Axis::RightTrigger);
  return mask;
  }

std::string deviceName(GCController* controller) {
  const char* name = controller.vendorName.UTF8String;
  return name == nullptr ? "Game controller" : name;
  }
}

MacosController::MacosController(GCController* controller, bool active)
  : GameController::Impl(deviceName(controller), supportedButtons(controller), supportedAxes(controller),
                         sample(controller.extendedGamepad), active), native([controller retain]) {
  }

void MacosController::bind(const std::shared_ptr<MacosController>& self) {
  assert([NSThread isMainThread]);
  // Blocks copy weak_ptr itself, never a reference to a returned stack frame.
  std::weak_ptr<MacosController> weak = self;
  const auto b = buttons(native.extendedGamepad);
  for(size_t i = 0; i < b.size(); ++i) {
    b[i].valueChangedHandler = ^(GCControllerButtonInput*, float value, BOOL pressed) {
      if(auto state = weak.lock())
        state->button(Button(i), pressed, value);
      };
    }
  const auto a = stickAxes(native.extendedGamepad);
  for(size_t i = 0; i < a.size(); ++i) {
    a[i].valueChangedHandler = ^(GCControllerAxisInput*, float value) {
      if(auto state = weak.lock())
        state->axis(Axis(i), value);
      };
    }
  }

void MacosController::feedSample() {
  assert([NSThread isMainThread]);
  const auto state = sample(native.extendedGamepad);
  for(size_t i = 0; i < size_t(Axis::Count); ++i)
    axis(Axis(i), state.axes[i]);
  for(size_t i = 0; i < size_t(Button::Count); ++i) {
    const auto value = Button(i) == Button::LeftTrigger ? state.axes[size_t(Axis::LeftTrigger)] :
                       Button(i) == Button::RightTrigger ? state.axes[size_t(Axis::RightTrigger)] : 0.f;
    button(Button(i), (state.buttons & (uint32_t(1) << i)) != 0, value);
    }
  }

void MacosController::setActive(bool active) {
  // Window focus notifications can precede the native recovery notification.
  if(active)
    feedSample();
  GameController::Impl::setActive(active);
  }

void MacosController::disconnect() {
  assert([NSThread isMainThread]);
  if(native == nil)
    return;
  for(auto* button : buttons(native.extendedGamepad))
    button.valueChangedHandler = nil;
  for(auto* axis : stickAxes(native.extendedGamepad))
    axis.valueChangedHandler = nil;
  GameController::Impl::disconnect();
  [native release];
  native = nil;
  }

struct MacosControllerApi::Impl {
  std::vector<std::shared_ptr<MacosController>> devices;
  NSMutableArray* observers = [[NSMutableArray alloc] init];
  bool active = NSApp.isActive;

  ~Impl() {
    assert([NSThread isMainThread]);
    for(id observer in observers)
      [[NSNotificationCenter defaultCenter] removeObserver:observer];
    [observers release];
    for(auto& device : devices)
      device->disconnect();
    }

  void setActive(bool next) {
    assert([NSThread isMainThread]);
    if(next == active)
      return;
    active = next;
    for(const auto& device : devices)
      device->setActive(active);
    }

  void connect(GCController* controller) {
    assert([NSThread isMainThread]);
    if(controller.extendedGamepad == nil)
      return;
    for(const auto& device : devices)
      if(device->native == controller)
        return;
    controller.handlerQueue = dispatch_get_main_queue();
    auto input = std::shared_ptr<MacosController>(new MacosController(controller, active));
    input->bind(input);
    devices.push_back(std::move(input));
    }

  void disconnect(GCController* controller) {
    assert([NSThread isMainThread]);
    auto it = std::find_if(devices.begin(), devices.end(), [controller](const auto& device) {
      return device->native == controller;
      });
    if(it == devices.end())
      return;
    (*it)->disconnect();
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

MacosControllerApi::MacosControllerApi() {
  requireMainThread();
  // Do not access weak-linked classes or notification constants before this guard.
  if(@available(macOS 11.0, *)) {
    if(NSClassFromString(@"GCController") != Nil) {
      impl = std::make_shared<Impl>();
      impl->observe(impl);
      }
    }
  }

MacosControllerApi::~MacosControllerApi() = default;

void MacosControllerApi::requireMainThread() {
  if(![NSThread isMainThread])
    throw std::logic_error("Application::controllers must be called on the macOS main thread");
  }

std::vector<std::shared_ptr<MacosController>> MacosControllerApi::controllers() const {
  requireMainThread();
  return impl ? impl->devices : std::vector<std::shared_ptr<MacosController>>{};
  }

#endif
