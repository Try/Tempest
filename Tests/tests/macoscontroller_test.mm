// Standalone native target: do not also compile macoscontrollerapi.mm.
// Synthetic native profiles exercise installed handlers without a Tempest Window.
#include "../../Engine/system/api/controllerapi.h"
#include "../../Engine/system/controllerstate.h"

#include <gtest/gtest.h>

#include <array>
#include <future>
#include <optional>
#include <thread>

#define private public
#include "../../Engine/system/api/macoscontrollerapi.h"
#undef private
#include "../../Engine/system/api/macoscontrollerapi.mm"

using Tempest::Detail::ControllerApi;
using Tempest::Detail::ControllerState;
using Tempest::Detail::MacOSControllerApi;

namespace {
using Pad = Tempest::GameController;
using Event = Pad::Event;

std::vector<Event> drain(const Pad& controller) {
  std::vector<Event> result;
  controller.pull([&](const Event& event) { result.push_back(event); });
  return result;
  }

// This scope ends before callbacks execute, checking weak_ptr capture lifetime.
__attribute__((noinline))
void connectFromReturnedFrame(MacOSControllerApi& backend, GCController* controller) {
  backend.impl->connect(controller);
  }
}

TEST(MacOSController, NativeCallbacksCopiesDisconnectAndLifetime) {
  @autoreleasepool {
    ASSERT_TRUE([NSThread isMainThread]);
    auto backend = std::make_unique<MacOSControllerApi>();
    const auto existing = backend->impl->devices;
    for(const auto& device : existing)
      backend->impl->disconnect(device.controller);
    backend->impl->setActive(true);
    GCController* native = [GCController controllerWithExtendedGamepad];
    connectFromReturnedFrame(*backend, native);
    ASSERT_EQ(backend->controllers().size(), 1u);
    auto state = backend->controllers().front();
    auto controller = ControllerApi::controller(state);
    auto enumerated = ControllerApi::controller(backend->controllers().front());
    ASSERT_EQ(drain(controller).size(), 1u);
    EXPECT_TRUE(drain(enumerated).empty());
    EXPECT_EQ(&controller.device(), &enumerated.device());
    EXPECT_EQ(native.handlerQueue, dispatch_get_main_queue());

    const auto inputs = buttons(native.extendedGamepad);
    size_t supported = 0;
    for(size_t i = 0; i < inputs.size(); ++i) {
      if(inputs[i] == nil)
        continue;
      ++supported;
      auto handler = inputs[i].valueChangedHandler;
      ASSERT_NE(handler, nil);
      // Callback arguments deliberately differ from the unchanged profile.
      handler(inputs[i], 1.f, YES);
      handler(inputs[i], 0.f, NO);
      }
    std::thread consumer([copy = controller, supported] {
      const auto caller = std::this_thread::get_id();
      size_t count = 0;
      copy.pull([&](const Event& event) {
        EXPECT_EQ(std::this_thread::get_id(), caller);
        EXPECT_EQ(event.type, count % 2 == 0 ? Event::Type::ButtonDown : Event::Type::ButtonUp);
        EXPECT_EQ(event.device, copy.device().id);
        ++count;
        });
      EXPECT_EQ(count, supported * 2);
      EXPECT_EQ(copy.state().buttons, 0u);
      });
    consumer.join();

    const auto analog = stickAxes(native.extendedGamepad);
    for(size_t i = 0; i < analog.size(); ++i) {
      ASSERT_NE(analog[i].valueChangedHandler, nil);
      analog[i].valueChangedHandler(analog[i], i % 2 == 0 ? .75f : -.5f);
      }
    native.extendedGamepad.leftTrigger.valueChangedHandler(native.extendedGamepad.leftTrigger, .25f, NO);
    native.extendedGamepad.rightTrigger.valueChangedHandler(native.extendedGamepad.rightTrigger, .8f, NO);
    EXPECT_EQ(controller.state().axes, (std::array<float, 6>{{.75f, -.5f, .75f, -.5f, .25f, .8f}}));
    EXPECT_TRUE(drain(controller).empty());

    // App focus is independent of Tempest windows, and snapshots stay current.
    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationWillResignActiveNotification object:nil];
    auto focus = drain(controller);
    ASSERT_EQ(focus.size(), 1u);
    EXPECT_EQ(focus[0].reason, Event::ResetReason::FocusLost);
    EXPECT_EQ(focus[0].device, controller.device().id);
    inputs[0].valueChangedHandler(inputs[0], 1.f, YES);
    EXPECT_EQ(controller.state().buttons, uint32_t(1));
    EXPECT_TRUE(drain(controller).empty());
    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidBecomeActiveNotification object:nil];
    // Regain samples the real profile (which remains neutral in this fixture).
    EXPECT_EQ(controller.state().buttons, 0u);
    const auto gained = drain(controller);
    ASSERT_EQ(gained.size(), 1u);
    EXPECT_EQ(gained[0].reason, Event::ResetReason::FocusGained);

    for(size_t edge = 0; edge < ControllerState::MaxEvents; ++edge)
      inputs[0].valueChangedHandler(inputs[0], edge % 2 == 0 ? 1.f : 0.f, edge % 2 == 0);
    native.extendedGamepad.leftTrigger.valueChangedHandler(native.extendedGamepad.leftTrigger, .9f, YES);
    const auto overflow = drain(controller);
    ASSERT_EQ(overflow.size(), 1u);
    EXPECT_EQ(overflow[0].reason, Event::ResetReason::Overflow);
    EXPECT_EQ(overflow[0].device, controller.device().id);
    EXPECT_EQ(controller.state().axes[size_t(Pad::Axis::LeftTrigger)], .9f);
    EXPECT_NE(controller.state().buttons & (uint32_t(1) << uint32_t(Pad::Button::LeftTrigger)), 0u);

    // Already copied native callbacks must not resurrect a disconnected handle.
    auto oldButton = Block_copy(inputs[0].valueChangedHandler);
    auto oldAxis = Block_copy(analog[0].valueChangedHandler);
    const auto oldId = controller.device().id;
    backend->impl->disconnect(native);
    const auto disconnected = drain(controller);
    ASSERT_EQ(disconnected.size(), 2u);
    EXPECT_EQ(disconnected[0].reason, Event::ResetReason::Disconnected);
    EXPECT_EQ(disconnected[1].type, Event::Type::Disconnected);
    EXPECT_TRUE(backend->controllers().empty());
    oldButton(inputs[0], 1.f, YES);
    oldAxis(analog[0], 1.f);
    EXPECT_EQ(controller.state().buttons, 0u);
    EXPECT_EQ(controller.state().axes, Pad::State{}.axes);
    EXPECT_TRUE(drain(controller).empty());

    connectFromReturnedFrame(*backend, native);
    std::optional<Pad> live = ControllerApi::controller(backend->controllers().front());
    EXPECT_NE(live->device().id, oldId);
    auto liveButton = Block_copy(inputs[0].valueChangedHandler);
    auto liveAxis = Block_copy(analog[0].valueChangedHandler);
    std::weak_ptr<MacOSControllerApi::Impl> nativeLifetime = backend->impl;
    std::weak_ptr<ControllerState> liveState = backend->controllers().front();
    backend.reset();
    EXPECT_TRUE(nativeLifetime.expired());
    EXPECT_FALSE(liveState.expired());
    EXPECT_EQ(inputs[0].valueChangedHandler, nil);
    EXPECT_EQ(analog[0].valueChangedHandler, nil);
    oldButton(inputs[0], 0.f, NO);
    oldAxis(analog[0], 0.f);
    liveButton(inputs[0], 1.f, YES);
    liveAxis(analog[0], 1.f);
    EXPECT_EQ(live->state().buttons, 0u);
    EXPECT_EQ(live->state().axes, Pad::State{}.axes);
    const auto terminal = drain(*live);
    ASSERT_EQ(terminal.size(), 2u);
    EXPECT_EQ(terminal[1].type, Event::Type::Disconnected);
    std::thread releaseLastCopy([handle = std::move(live)]() mutable { handle.reset(); });
    live.reset();
    releaseLastCopy.join();
    EXPECT_TRUE(liveState.expired());
    Block_release(oldButton);
    Block_release(oldAxis);
    Block_release(liveButton);
    Block_release(liveAxis);
    }
  }

TEST(MacOSController, FocusGainRefreshPrecedesResetAfterEarlyWindowFocus) {
  @autoreleasepool {
    MacOSControllerApi backend;
    const auto existing = backend.impl->devices;
    for(const auto& device : existing)
      backend.impl->disconnect(device.controller);
    backend.impl->setActive(false);
    GCController* native = [GCController controllerWithExtendedGamepad];
    connectFromReturnedFrame(backend, native);
    auto controller = ControllerApi::controller(backend.controllers().front());
    drain(controller);
    auto* pad = native.extendedGamepad;
    [pad.buttonA setValue:1.f];
    [pad.leftThumbstick.xAxis setValue:.8f];
    [pad.rightThumbstick.yAxis setValue:-.7f];
    [pad.leftTrigger setValue:.65f];
    ASSERT_TRUE(pad.buttonA.isPressed);
    ASSERT_EQ(pad.leftThumbstick.xAxis.value, .8f);
    ASSERT_EQ(pad.rightThumbstick.yAxis.value, -.7f);
    ASSERT_EQ(pad.leftTrigger.value, .65f);

    // Model a window-focus notification observing synthetic neutral input before
    // the application-focus notification refreshes this held native profile.
    pad.buttonA.valueChangedHandler(pad.buttonA, 0.f, NO);
    pad.leftThumbstick.xAxis.valueChangedHandler(pad.leftThumbstick.xAxis, 0.f);
    pad.rightThumbstick.yAxis.valueChangedHandler(pad.rightThumbstick.yAxis, 0.f);
    pad.leftTrigger.valueChangedHandler(pad.leftTrigger, 0.f, NO);
    EXPECT_EQ(controller.state().buttons, 0u);
    EXPECT_EQ(controller.state().axes, Pad::State{}.axes);
    EXPECT_TRUE(drain(controller).empty());

    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidBecomeActiveNotification object:nil];
    size_t calls = 0;
    controller.pull([&](const Event& event) {
      EXPECT_EQ(event.type, Event::Type::Reset);
      EXPECT_EQ(event.reason, Event::ResetReason::FocusGained);
      EXPECT_EQ(event.device, controller.device().id);
      const auto refreshed = controller.state();
      EXPECT_NE(refreshed.buttons & uint32_t(1), 0u);
      EXPECT_EQ(refreshed.axes[size_t(Pad::Axis::LeftX)], .8f);
      EXPECT_EQ(refreshed.axes[size_t(Pad::Axis::RightY)], -.7f);
      EXPECT_EQ(refreshed.axes[size_t(Pad::Axis::LeftTrigger)], .65f);
      ++calls;
      });
    EXPECT_EQ(calls, 1u);
    }
  }

TEST(MacOSController, PublicEnumerationNeedsMainThreadButNoTempestWindow) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp finishLaunching];
    // The target links no SystemApi, Application, renderer or Window source.
    EXPECT_NO_THROW(Pad::controllers());
    std::thread worker([] {
      EXPECT_THROW(Pad::controllers(), std::logic_error);
      });
    worker.join();
    // Pump the host run loop, independently of Tempest window/event APIs.
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];
    }
  }
