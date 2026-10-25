#include "../../Engine/system/gamepadstate.h"

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

using Tempest::Gamepad;
using Tempest::Detail::GamepadState;

namespace {
using Button = Gamepad::Button;
using Axis = Gamepad::Axis;
using Event = Gamepad::Event;
constexpr uint32_t buttons = (uint32_t(1) << uint32_t(Button::Count)) - 1;
constexpr uint32_t axes = (uint32_t(1) << uint32_t(Axis::Count)) - 1;
constexpr uint32_t bit(Button value) { return uint32_t(1) << uint32_t(value); }
constexpr uint32_t bit(Axis value) { return uint32_t(1) << uint32_t(value); }

struct Input : testing::Test {
  GamepadState input;
  Gamepad::DeviceId id = 0;

  void SetUp() override {
    input.setActive(true);
    id = input.connect("controller", buttons, axes, {});
    Event event;
    ASSERT_TRUE(input.pollEvent(event));
    ASSERT_EQ(event.type, Event::Type::Connected);
    ASSERT_EQ(event.device, id);
    }

  Gamepad::State state() {
    Gamepad::State result;
    EXPECT_TRUE(input.state(id, result));
    return result;
    }

  Event next(Event::Type type) {
    Event result;
    EXPECT_TRUE(input.pollEvent(result));
    EXPECT_EQ(result.type, type);
    return result;
    }

  void empty() {
    Event event;
    EXPECT_FALSE(input.pollEvent(event));
    }
};
}

TEST_F(Input, ShortEdgesAndDuplicates) {
  input.button(id, Button::South, true);
  input.button(id, Button::South, true);
  input.button(id, Button::East, true);
  input.button(id, Button::South, false);
  input.button(id, Button::South, false);
  EXPECT_EQ(state().buttons, bit(Button::East));
  EXPECT_EQ(next(Event::Type::ButtonDown).button, Button::South);
  EXPECT_EQ(next(Event::Type::ButtonDown).button, Button::East);
  EXPECT_EQ(next(Event::Type::ButtonUp).button, Button::South);
  empty();
  }

TEST_F(Input, ConnectSuppressesInitiallyHeldElementsIndependently) {
  Gamepad::State initial;
  initial.buttons = bit(Button::South);
  initial.axes[size_t(Axis::LeftX)] = 0.8f;
  const auto second = input.connect("held", buttons, axes, initial);
  next(Event::Type::Connected);
  input.button(second, Button::South, true);
  input.axis(second, Axis::LeftX, 0.7f);
  input.button(second, Button::East, true);
  input.axis(second, Axis::RightY, 0.02f);
  Gamepad::State value;
  ASSERT_TRUE(input.state(second, value));
  EXPECT_EQ(value.buttons, bit(Button::East));
  EXPECT_EQ(value.axes[size_t(Axis::LeftX)], 0.f);
  EXPECT_EQ(value.axes[size_t(Axis::RightY)], 0.02f);
  EXPECT_EQ(next(Event::Type::ButtonDown).button, Button::East);
  input.button(second, Button::South, false);
  input.axis(second, Axis::LeftX, 0.1f);
  empty();
  input.button(second, Button::South, true);
  input.axis(second, Axis::LeftX, -0.03f);
  next(Event::Type::ButtonDown);
  ASSERT_TRUE(input.state(second, value));
  EXPECT_EQ(value.axes[size_t(Axis::LeftX)], -0.03f);
  }

TEST_F(Input, FocusCancelsPendingEdgesAndRequiresNeutralSample) {
  input.button(id, Button::South, true);
  input.axis(id, Axis::LeftX, 0.7f);
  input.setActive(false);
  EXPECT_EQ(state().buttons, 0u);
  EXPECT_EQ(state().axes[size_t(Axis::LeftX)], 0.f);
  EXPECT_EQ(next(Event::Type::Reset).reason, Event::ResetReason::FocusLost);
  input.setActive(false);
  input.button(id, Button::East, true);
  input.axis(id, Axis::RightY, 1.f);
  empty();
  input.setActive(true);
  input.button(id, Button::South, true);
  input.axis(id, Axis::LeftX, 0.7f);
  empty();
  input.button(id, Button::East, false);
  input.button(id, Button::East, true);
  input.axis(id, Axis::RightY, 0.f);
  input.axis(id, Axis::RightY, 0.01f);
  next(Event::Type::ButtonDown);
  EXPECT_EQ(state().buttons, bit(Button::East));
  EXPECT_EQ(state().axes[size_t(Axis::LeftX)], 0.f);
  EXPECT_EQ(state().axes[size_t(Axis::RightY)], 0.01f);
  input.button(id, Button::South, false);
  input.button(id, Button::South, true);
  next(Event::Type::ButtonDown);
  }

TEST_F(Input, ConnectWhileInactiveRequiresNeutralAfterActivation) {
  input.setActive(false);
  next(Event::Type::Reset);
  const auto second = input.connect("background", buttons, axes, {});
  next(Event::Type::Connected);
  input.setActive(true);
  input.button(second, Button::South, true);
  input.axis(second, Axis::LeftX, 1.f);
  empty();
  input.button(second, Button::South, false);
  input.button(second, Button::South, true);
  input.axis(second, Axis::LeftX, 0.f);
  input.axis(second, Axis::LeftX, 0.3f);
  next(Event::Type::ButtonDown);
  Gamepad::State value;
  ASSERT_TRUE(input.state(second, value));
  EXPECT_EQ(value.axes[size_t(Axis::LeftX)], 0.3f);
  }

TEST_F(Input, DisconnectCancelsOnlyItsPendingEdgesAndStaleId) {
  const auto second = input.connect("second", buttons, axes, {});
  next(Event::Type::Connected);
  input.button(id, Button::South, true);
  input.button(second, Button::East, true);
  input.disconnect(id);
  EXPECT_EQ(next(Event::Type::ButtonDown).device, second);
  auto reset = next(Event::Type::Reset);
  EXPECT_EQ(reset.device, id);
  EXPECT_EQ(reset.reason, Event::ResetReason::Disconnected);
  EXPECT_EQ(next(Event::Type::Disconnected).device, id);
  Gamepad::State value;
  value.buttons = buttons;
  value.axes.fill(1.f);
  EXPECT_FALSE(input.state(id, value));
  EXPECT_EQ(value.buttons, 0u);
  for(float axis: value.axes)
    EXPECT_EQ(axis, 0.f);
  input.button(id, Button::South, true);
  input.axis(id, Axis::LeftX, 1.f);
  input.disconnect(id);
  empty();
  const auto replacement = input.connect("controller", buttons, axes, {});
  EXPECT_NE(replacement, id);
  EXPECT_NE(replacement, second);
  EXPECT_NE(replacement, 0u);
  ASSERT_EQ(input.devices().size(), 2u);
  EXPECT_EQ(input.devices()[0].id, second);
  }

TEST_F(Input, AxisClampingFiniteAndNoGameplayDeadZone) {
  input.axis(id, Axis::LeftX, -2.f);
  input.axis(id, Axis::LeftY, 2.f);
  input.axis(id, Axis::RightX, 0.001f);
  input.axis(id, Axis::LeftTrigger, -1.f);
  input.axis(id, Axis::RightTrigger, 2.f);
  auto value = state();
  EXPECT_EQ(value.axes[size_t(Axis::LeftX)], -1.f);
  EXPECT_EQ(value.axes[size_t(Axis::LeftY)], 1.f);
  EXPECT_EQ(value.axes[size_t(Axis::RightX)], 0.001f);
  EXPECT_EQ(value.axes[size_t(Axis::LeftTrigger)], 0.f);
  EXPECT_EQ(value.axes[size_t(Axis::RightTrigger)], 1.f);
  for(float invalid: {std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity()}) {
    input.axis(id, Axis::RightX, invalid);
    EXPECT_EQ(state().axes[size_t(Axis::RightX)], 0.001f);
    }
  empty();
  }

TEST_F(Input, UnsupportedAndInvalidControlsIgnored) {
  const auto limited = input.connect("limited", bit(Button::South) | 0x80000000u,
                                     bit(Axis::LeftX) | 0x80000000u, {});
  next(Event::Type::Connected);
  input.button(limited, Button::East, true);
  input.button(limited, Button::Count, true);
  input.button(limited, Button(255), true);
  input.axis(limited, Axis::RightX, 1.f);
  input.axis(limited, Axis::Count, 1.f);
  input.axis(limited, Axis(255), 1.f);
  Gamepad::State value;
  ASSERT_TRUE(input.state(limited, value));
  EXPECT_EQ(value.buttons, 0u);
  for(float axis: value.axes)
    EXPECT_EQ(axis, 0.f);
  empty();
  const auto devices = input.devices();
  ASSERT_EQ(devices.size(), 2u);
  EXPECT_EQ(devices[1].supportedButtons, bit(Button::South));
  EXPECT_EQ(devices[1].supportedAxes, bit(Axis::LeftX));
  EXPECT_EQ(devices[1].name, "limited");
  }

TEST_F(Input, ButtonOverflowCancelsAllAndTriggeringEdge) {
  const auto second = input.connect("second", buttons, axes, {});
  next(Event::Type::Connected);
  input.axis(second, Axis::RightX, 0.5f);
  for(size_t i = 0; i < GamepadState::MaxEvents; ++i)
    input.button(id, Button::South, i % 2 == 0);
  input.button(id, Button::South, true);
  auto reset = next(Event::Type::Reset);
  EXPECT_EQ(reset.device, 0u);
  EXPECT_EQ(reset.reason, Event::ResetReason::Overflow);
  empty();
  EXPECT_EQ(state().buttons, 0u);
  Gamepad::State value;
  ASSERT_TRUE(input.state(second, value));
  EXPECT_EQ(value.axes[size_t(Axis::RightX)], 0.f);
  input.button(id, Button::South, true);
  input.button(second, Button::East, true);
  input.axis(second, Axis::RightX, 0.5f);
  input.axis(second, Axis::RightY, 0.8f);
  const auto fresh = next(Event::Type::ButtonDown);
  EXPECT_EQ(fresh.device, second);
  EXPECT_EQ(fresh.button, Button::East);
  ASSERT_TRUE(input.state(second, value));
  EXPECT_EQ(value.buttons, bit(Button::East));
  EXPECT_EQ(value.axes[size_t(Axis::RightX)], 0.f);
  EXPECT_EQ(value.axes[size_t(Axis::RightY)], 0.8f);
  empty();
  input.button(id, Button::South, false);
  input.button(id, Button::South, true);
  input.axis(second, Axis::RightX, 0.1f);
  input.axis(second, Axis::RightX, 0.02f);
  next(Event::Type::ButtonDown);
  ASSERT_TRUE(input.state(second, value));
  EXPECT_EQ(value.axes[size_t(Axis::RightX)], 0.02f);
  }

TEST_F(Input, LifecycleOverflowRetainsAuthoritativeDevices) {
  for(size_t i = 0; i < GamepadState::MaxEvents + 1; ++i)
    input.connect("new", 0, 0, {});
  auto reset = next(Event::Type::Reset);
  EXPECT_EQ(reset.device, 0u);
  EXPECT_EQ(reset.reason, Event::ResetReason::Overflow);
  empty();
  EXPECT_EQ(input.devices().size(), GamepadState::MaxEvents + 2);
  }

TEST_F(Input, DisconnectOverflowStillResetsBeforeDisconnect) {
  for(size_t i = 0; i < GamepadState::MaxEvents; ++i)
    input.connect("new", 0, 0, {});
  input.disconnect(id);
  auto reset = next(Event::Type::Reset);
  EXPECT_EQ(reset.device, 0u);
  EXPECT_EQ(reset.reason, Event::ResetReason::Overflow);
  EXPECT_EQ(next(Event::Type::Disconnected).device, id);
  empty();
  }

TEST_F(Input, FocusCancelsCompletePendingTapAndOtherDeviceStates) {
  const auto second = input.connect("second", buttons, axes, {});
  next(Event::Type::Connected);
  input.button(id, Button::South, true);
  input.button(id, Button::South, false);
  input.button(second, Button::East, true);
  input.setActive(false);
  EXPECT_EQ(next(Event::Type::Reset).device, id);
  EXPECT_EQ(next(Event::Type::Reset).device, second);
  empty();
  Gamepad::State value;
  ASSERT_TRUE(input.state(second, value));
  EXPECT_EQ(value.buttons, 0u);
  }

TEST_F(Input, QueuePreservesAll1024EdgesAtCapacity) {
  for(size_t i = 0; i < GamepadState::MaxEvents; ++i)
    input.button(id, Button::South, i % 2 == 0);
  for(size_t i = 0; i < GamepadState::MaxEvents; ++i) {
    const auto event = next(i % 2 == 0 ? Event::Type::ButtonDown : Event::Type::ButtonUp);
    EXPECT_EQ(event.device, id);
    EXPECT_EQ(event.button, Button::South);
    }
  empty();
  EXPECT_EQ(state().buttons, 0u);
  }

TEST_F(Input, InvalidAxisCannotRearmBlockedControl) {
  input.setActive(false);
  next(Event::Type::Reset);
  input.setActive(true);
  input.axis(id, Axis::LeftX, std::numeric_limits<float>::quiet_NaN());
  input.axis(id, Axis::LeftX, std::numeric_limits<float>::infinity());
  input.axis(id, Axis::LeftX, 0.8f);
  EXPECT_EQ(state().axes[size_t(Axis::LeftX)], 0.f);
  input.axis(id, Axis::LeftX, -0.1f);
  input.axis(id, Axis::LeftX, -0.01f);
  EXPECT_EQ(state().axes[size_t(Axis::LeftX)], -0.01f);
  empty();
  }


TEST_F(Input, OverflowTriggeredByReleaseKeepsReleasedButtonArmed) {
  input.button(id, Button::South, true);
  next(Event::Type::ButtonDown);
  for(size_t i = 0; i < GamepadState::MaxEvents; ++i)
    input.connect("new", 0, 0, {});
  input.button(id, Button::South, false);
  const auto reset = next(Event::Type::Reset);
  EXPECT_EQ(reset.device, 0u);
  EXPECT_EQ(reset.reason, Event::ResetReason::Overflow);
  EXPECT_EQ(state().buttons, 0u);
  empty();
  input.button(id, Button::South, true);
  EXPECT_EQ(next(Event::Type::ButtonDown).button, Button::South);
  EXPECT_EQ(state().buttons, bit(Button::South));
  empty();
  }

TEST_F(Input, OverflowUsesLatestRawSampleEvenWhileSuppressed) {
  Gamepad::State initial;
  initial.buttons = bit(Button::East);
  initial.axes[size_t(Axis::RightY)] = std::numeric_limits<float>::quiet_NaN();
  const auto second = input.connect("held", buttons, axes, initial);
  next(Event::Type::Connected);
  for(size_t i = 0; i <= GamepadState::MaxEvents; ++i)
    input.button(id, Button::South, i % 2 == 0);
  next(Event::Type::Reset);
  input.button(second, Button::East, true);
  input.axis(second, Axis::RightY, 0.8f);
  empty();
  Gamepad::State value;
  ASSERT_TRUE(input.state(second, value));
  EXPECT_EQ(value.buttons, 0u);
  EXPECT_EQ(value.axes[size_t(Axis::RightY)], 0.f);
  input.button(second, Button::East, false);
  input.axis(second, Axis::RightY, 0.f);
  // Rearmed samples must survive a later overflow without another neutral callback.
  for(size_t i = 0; i <= GamepadState::MaxEvents; ++i)
    input.connect("new", 0, 0, {});
  next(Event::Type::Reset);
  input.button(second, Button::East, true);
  input.axis(second, Axis::RightY, 0.8f);
  EXPECT_EQ(next(Event::Type::ButtonDown).device, second);
  ASSERT_TRUE(input.state(second, value));
  EXPECT_EQ(value.buttons, bit(Button::East));
  EXPECT_EQ(value.axes[size_t(Axis::RightY)], 0.8f);
  empty();
  }
