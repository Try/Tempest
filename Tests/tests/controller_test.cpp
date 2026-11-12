#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <Tempest/Vec>

#include "controller_test_access.h"

using Tempest::GameController;
using Tempest::Detail::GameControllerTestAccess;

namespace {
using Button = GameController::Button;
using Axis = GameController::Axis;
using Event = GameController::Event;
constexpr uint32_t buttons = ((uint32_t(1) << uint32_t(Button::Count)) - 1) & ~uint32_t(1);
constexpr uint32_t axes = (uint32_t(1) << uint32_t(Axis::Count)) - 1;
constexpr uint32_t bit(Button value) { return uint32_t(1) << uint32_t(value); }
constexpr uint32_t bit(Axis value) { return uint32_t(1) << uint32_t(value); }

struct ControllerState final : GameControllerTestAccess::Impl {
  ControllerState(std::string name, uint32_t buttons, uint32_t axes,
                  const GameController::State& initial, bool active = true)
    : Impl(std::move(name), buttons, axes, initial, active) {}

  using Impl::MaxEvents;
  using Impl::disconnect;
  using Impl::setActive;
  using Impl::button;
  using Impl::axis;
  };

std::vector<Event> drain(const GameController& controller) {
  std::vector<Event> result;
  controller.pull([&](const Event& event) {
    if(event.type != Event::Type::ButtonDown && event.type != Event::Type::ButtonUp)
      EXPECT_EQ(event.button, Button::None);
    result.push_back(event);
    });
  return result;
  }

struct Controller : testing::Test {
  std::shared_ptr<ControllerState> input = std::make_shared<ControllerState>("controller", buttons, axes,
                                                                           GameController::State{});
  GameController controller = GameControllerTestAccess::make(input);

  void SetUp() override {
    const auto events = drain(controller);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, Event::Type::Connected);
    EXPECT_EQ(events[0].button, Button::None);
    }

  void button(Button button, bool pressed) {
    input->button(button, pressed, pressed ? 1.f : 0.f);
    }
  };
}

TEST_F(Controller, CopiesAndEnumerationShareIdentityQueueAndShortTaps) {
  auto copy = controller;
  auto enumerated = GameControllerTestAccess::make(input);
  EXPECT_EQ(copy, enumerated);
  EXPECT_FALSE(copy != enumerated);
  EXPECT_EQ(&copy.device(), &enumerated.device());
  EXPECT_EQ(controller.device().name, "controller");
  EXPECT_EQ(controller.device().supportedButtons, buttons);
  EXPECT_EQ(controller.device().supportedAxes, axes);
  button(Button::South, true);
  button(Button::South, true);
  button(Button::East, true);
  button(Button::South, false);
  button(Button::South, false);
  const auto events = drain(copy);
  ASSERT_EQ(events.size(), 3u);
  EXPECT_EQ(events[0].type, Event::Type::ButtonDown);
  EXPECT_EQ(events[0].button, Button::South);
  EXPECT_EQ(events[1].button, Button::East);
  EXPECT_EQ(events[2].type, Event::Type::ButtonUp);
  EXPECT_EQ(events[2].button, Button::South);
  EXPECT_EQ(enumerated.state().buttons, bit(Button::East));
  EXPECT_TRUE(drain(controller).empty());
  EXPECT_TRUE(drain(enumerated).empty());
  }

TEST_F(Controller, CallbackRunsOnCallerThreadAndCanReadState) {
  button(Button::South, true);
  std::thread consumer([copy = controller] {
    const auto caller = std::this_thread::get_id();
    size_t calls = 0;
    copy.pull([&](const Event& event) {
      EXPECT_EQ(std::this_thread::get_id(), caller);
      EXPECT_EQ(event.type, Event::Type::ButtonDown);
      EXPECT_EQ(copy.state().buttons, bit(Button::South));
      EXPECT_EQ(copy.axis(GameController::Stick::Left), Tempest::Vec2{});
      ++calls;
      });
    EXPECT_EQ(calls, 1u);
    });
  consumer.join();
  EXPECT_TRUE(drain(controller).empty());
  }

TEST_F(Controller, CallbackExceptionPreservesUntouchedEvents) {
  button(Button::South, true);
  button(Button::South, false);
  EXPECT_THROW(controller.pull([&](const Event&) {
    button(Button::East, true);
    throw std::runtime_error("callback");
    }), std::runtime_error);
  const auto events = drain(controller);
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].type, Event::Type::ButtonUp);
  EXPECT_EQ(events[0].button, Button::South);
  EXPECT_EQ(events[1].type, Event::Type::ButtonDown);
  EXPECT_EQ(events[1].button, Button::East);
  EXPECT_TRUE(drain(controller).empty());
  }

TEST_F(Controller, FocusLossInsideCallbackCancelsRemainingEdges) {
  button(Button::South, true);
  button(Button::South, false);
  button(Button::East, true);
  std::vector<Event> observed;
  controller.pull([&](const Event& event) {
    observed.push_back(event);
    if(observed.size() == 1)
      input->setActive(false);
    });
  ASSERT_EQ(observed.size(), 2u);
  EXPECT_EQ(observed[0].type, Event::Type::ButtonDown);
  EXPECT_EQ(observed[1].type, Event::Type::Reset);
  EXPECT_EQ(observed[1].reason, Event::ResetReason::FocusLost);
  EXPECT_TRUE(drain(controller).empty());
  }

TEST_F(Controller, DisconnectInsideCallbackReplacesRemainingEdges) {
  button(Button::South, true);
  button(Button::South, false);
  button(Button::East, true);
  std::vector<Event> observed;
  controller.pull([&](const Event& event) {
    observed.push_back(event);
    if(observed.size() == 1)
      input->disconnect();
    });
  ASSERT_EQ(observed.size(), 3u);
  EXPECT_EQ(observed[1].reason, Event::ResetReason::Disconnected);
  EXPECT_EQ(observed[2].type, Event::Type::Disconnected);
  EXPECT_EQ(controller.state().buttons, 0u);
  EXPECT_TRUE(drain(controller).empty());
  }

TEST_F(Controller, OverflowInsideCallbackCancelsRemainingEdges) {
  button(Button::South, true);
  button(Button::South, false);
  button(Button::East, true);
  std::vector<Event> observed;
  controller.pull([&](const Event& event) {
    observed.push_back(event);
    if(observed.size() == 1)
      for(size_t i = 0; i < ControllerState::MaxEvents - 1; ++i)
        button(Button::South, i % 2 == 0);
    });
  ASSERT_EQ(observed.size(), 2u);
  EXPECT_EQ(observed[1].type, Event::Type::Reset);
  EXPECT_EQ(observed[1].reason, Event::ResetReason::Overflow);
  EXPECT_TRUE(drain(controller).empty());
  }

TEST_F(Controller, CancellationAndThrowPreserveReplacementQueue) {
  button(Button::South, true);
  button(Button::South, false);
  EXPECT_THROW(controller.pull([&](const Event&) {
    input->disconnect();
    throw std::runtime_error("callback");
    }), std::runtime_error);
  const auto events = drain(controller);
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].reason, Event::ResetReason::Disconnected);
  EXPECT_EQ(events[1].type, Event::Type::Disconnected);
  }

TEST_F(Controller, MovedFromAndEmptyHandlesAreSafe) {
  const auto copy = controller;
  auto moved = std::move(controller);
  EXPECT_EQ(moved, copy);
  EXPECT_NE(controller, moved);
  EXPECT_TRUE(controller != moved);
  EXPECT_TRUE(controller.device().name.empty());
  EXPECT_EQ(controller.device().supportedButtons, 0u);
  EXPECT_EQ(controller.device().supportedAxes, 0u);
  EXPECT_EQ(controller.state().buttons, 0u);
  EXPECT_EQ(controller.state().axes, GameController::State{}.axes);
  EXPECT_EQ(controller.axis(GameController::Stick::Left), Tempest::Vec2{});
  EXPECT_EQ(controller.axis(GameController::Stick::Right), Tempest::Vec2{});
  controller.pull([](const Event&) { FAIL() << "Moved-from handle delivered an event"; });
  auto empty = GameControllerTestAccess::make(nullptr);
  EXPECT_EQ(empty, controller);
  EXPECT_EQ(controller, empty);
  EXPECT_FALSE(empty != controller);
  EXPECT_EQ(empty.state().buttons, 0u);
  EXPECT_EQ(empty.state().axes, GameController::State{}.axes);
  EXPECT_EQ(empty.axis(GameController::Stick::Left), Tempest::Vec2{});
  empty.pull([](const Event&) { FAIL() << "Empty handle delivered an event"; });
  }

TEST_F(Controller, PullIncludesFiniteInputAddedByCallback) {
  button(Button::South, true);
  std::vector<Event> observed;
  controller.pull([&](const Event& event) {
    observed.push_back(event);
    if(observed.size() == 1) {
      button(Button::South, false);
      button(Button::East, true);
      }
    });
  ASSERT_EQ(observed.size(), 3u);
  EXPECT_EQ(observed[0].type, Event::Type::ButtonDown);
  EXPECT_EQ(observed[0].button, Button::South);
  EXPECT_EQ(observed[1].type, Event::Type::ButtonUp);
  EXPECT_EQ(observed[1].button, Button::South);
  EXPECT_EQ(observed[2].type, Event::Type::ButtonDown);
  EXPECT_EQ(observed[2].button, Button::East);
  EXPECT_TRUE(drain(controller).empty());
  }

TEST_F(Controller, PullIncludesTerminalEventsAddedByCallback) {
  button(Button::South, true);
  std::vector<Event> observed;
  controller.pull([&](const Event& event) {
    observed.push_back(event);
    if(observed.size() == 1)
      input->disconnect();
    });
  ASSERT_EQ(observed.size(), 3u);
  EXPECT_EQ(observed[0].type, Event::Type::ButtonDown);
  EXPECT_EQ(observed[1].type, Event::Type::Reset);
  EXPECT_EQ(observed[1].button, Button::None);
  EXPECT_EQ(observed[1].reason, Event::ResetReason::Disconnected);
  EXPECT_EQ(observed[2].type, Event::Type::Disconnected);
  EXPECT_EQ(observed[2].button, Button::None);
  EXPECT_TRUE(drain(controller).empty());
  }

TEST(ControllerLifetime, CallbackMayDestroyLastHandle) {
  auto input = std::make_shared<ControllerState>("controller", buttons, axes, GameController::State{});
  std::weak_ptr<ControllerState> weak = input;
  std::optional<GameController> handle = GameControllerTestAccess::make(input);
  input.reset();
  bool called = false;
  handle->pull([&](const Event&) {
    handle.reset();
    EXPECT_FALSE(weak.expired());
    called = true;
    });
  EXPECT_TRUE(called);
  EXPECT_TRUE(weak.expired());
  }

TEST_F(Controller, AxesAreFiniteClampedAndHaveNoGameDeadZone) {
  input->axis(Axis::LeftX, -2.f);
  input->axis(Axis::LeftY, 2.f);
  input->axis(Axis::RightX, .001f);
  input->axis(Axis::LeftTrigger, -1.f);
  input->axis(Axis::RightTrigger, 2.f);
  EXPECT_EQ(controller.axis(GameController::Stick::Left), Tempest::Vec2(-1.f, 1.f));
  EXPECT_EQ(controller.axis(GameController::Stick::Right), Tempest::Vec2(.001f, 0.f));
  EXPECT_EQ(controller.axis(GameController::Stick(255)), Tempest::Vec2{});
  auto value = controller.state();
  EXPECT_EQ(value.axes[size_t(Axis::LeftTrigger)], 0.f);
  EXPECT_EQ(value.axes[size_t(Axis::RightTrigger)], 1.f);
  for(float invalid : {std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity(),
                       -std::numeric_limits<float>::infinity()}) {
    input->axis(Axis::RightX, invalid);
    EXPECT_EQ(controller.state().axes[size_t(Axis::RightX)], .001f);
    }
  EXPECT_TRUE(drain(controller).empty());
  }

TEST(ControllerCapabilities, InitialRawStateAndUnsupportedControls) {
  GameController::State initial;
  initial.buttons = buttons;
  initial.axes.fill(2.f);
  initial.axes[size_t(Axis::RightY)] = std::numeric_limits<float>::quiet_NaN();
  auto input = std::make_shared<ControllerState>("limited", bit(Button::South) | 0x80000000u,
                                                bit(Axis::LeftX) | bit(Axis::RightY) | 0x80000000u, initial);
  auto controller = GameControllerTestAccess::make(input);
  drain(controller);
  EXPECT_EQ(controller.device().supportedButtons, bit(Button::South));
  EXPECT_EQ(controller.device().supportedAxes, bit(Axis::LeftX) | bit(Axis::RightY));
  EXPECT_EQ(controller.state().buttons, bit(Button::South));
  EXPECT_EQ(controller.state().axes[size_t(Axis::LeftX)], 1.f);
  EXPECT_EQ(controller.state().axes[size_t(Axis::RightY)], 0.f);
  input->button(Button::East, true, 1.f);
  input->button(Button::Count, true, 1.f);
  input->button(Button(255), true, 1.f);
  input->axis(Axis::RightX, 1.f);
  input->axis(Axis::Count, 1.f);
  input->axis(Axis(255), 1.f);
  EXPECT_TRUE(drain(controller).empty());
  EXPECT_EQ(controller.state().axes[size_t(Axis::RightX)], 0.f);
  // The engine does not require an initially held control to return to neutral.
  input->axis(Axis::LeftX, .7f);
  EXPECT_EQ(controller.state().axes[size_t(Axis::LeftX)], .7f);
  }

TEST(ControllerCapabilities, NoneIsNotAControlOrCapability) {
  EXPECT_EQ(uint8_t(Button::None), 0u);
  const Event empty;
  EXPECT_EQ(empty.type, Event::Type::Reset);
  EXPECT_EQ(empty.button, Button::None);
  EXPECT_EQ(empty.reason, Event::ResetReason::None);
  GameController::State initial;
  initial.buttons = bit(Button::None) | bit(Button::Home);
  auto input = std::make_shared<ControllerState>("controller", bit(Button::None) | bit(Button::Home),
                                                axes, initial);
  auto controller = GameControllerTestAccess::make(input);
  drain(controller);
  EXPECT_EQ(controller.device().supportedButtons, bit(Button::Home));
  EXPECT_EQ(controller.state().buttons, bit(Button::Home));
  input->button(Button::None, true, 1.f);
  input->button(Button::None, false, 0.f);
  EXPECT_EQ(controller.state().buttons, bit(Button::Home));
  EXPECT_TRUE(drain(controller).empty());
  }

TEST_F(Controller, FocusLossCancelsHistoryAndInactiveSnapshotStaysCurrent) {
  button(Button::South, true);
  button(Button::South, false);
  input->axis(Axis::LeftX, .7f);
  input->setActive(false);
  input->setActive(false);
  button(Button::East, true);
  input->axis(Axis::LeftX, .8f);
  const auto events = drain(controller);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].reason, Event::ResetReason::FocusLost);
  EXPECT_EQ(controller.state().buttons, bit(Button::East));
  EXPECT_EQ(controller.state().axes[size_t(Axis::LeftX)], .8f);
  input->setActive(true);
  const auto gained = drain(controller);
  ASSERT_EQ(gained.size(), 1u);
  EXPECT_EQ(gained[0].reason, Event::ResetReason::FocusGained);
  input->setActive(true);
  EXPECT_TRUE(drain(controller).empty());
  button(Button::South, true);
  EXPECT_EQ(drain(controller).size(), 1u);
  }

TEST_F(Controller, FocusRecoveryResetFollowsRefreshedHeldSample) {
  input->setActive(false);
  drain(controller);
  // A window-focus consumer can first see Apple's synthetic neutral sample.
  const auto earlyWindowFocus = controller.state();
  EXPECT_EQ(earlyWindowFocus.buttons, 0u);
  EXPECT_EQ(earlyWindowFocus.axes, GameController::State{}.axes);
  // The native manager refreshes raw input while still inactive.
  button(Button::South, true);
  input->axis(Axis::LeftX, .8f);
  input->axis(Axis::RightY, -.7f);
  input->button(Button::LeftTrigger, true, .65f);
  input->setActive(true);
  size_t calls = 0;
  controller.pull([&](const Event& event) {
    EXPECT_EQ(event.type, Event::Type::Reset);
    EXPECT_EQ(event.reason, Event::ResetReason::FocusGained);
    EXPECT_EQ(event.button, Button::None);
    const auto refreshed = controller.state();
    EXPECT_EQ(refreshed.buttons, bit(Button::South) | bit(Button::LeftTrigger));
    EXPECT_EQ(refreshed.axes[size_t(Axis::LeftX)], .8f);
    EXPECT_EQ(refreshed.axes[size_t(Axis::RightY)], -.7f);
    EXPECT_EQ(refreshed.axes[size_t(Axis::LeftTrigger)], .65f);
    ++calls;
    });
  EXPECT_EQ(calls, 1u);
  }

TEST_F(Controller, QueueRetains1024EdgesAndOverflowIsIsolated) {
  auto otherInput = std::make_shared<ControllerState>("other", buttons, axes, GameController::State{});
  auto other = GameControllerTestAccess::make(otherInput);
  drain(other);
  otherInput->button(Button::East, true, 1.f);
  for(size_t i = 0; i < ControllerState::MaxEvents; ++i)
    button(Button::South, i % 2 == 0);
  const auto complete = drain(controller);
  ASSERT_EQ(complete.size(), ControllerState::MaxEvents);
  for(size_t i = 0; i < complete.size(); ++i)
    EXPECT_EQ(complete[i].type, i % 2 == 0 ? Event::Type::ButtonDown : Event::Type::ButtonUp);
  for(size_t i = 0; i <= ControllerState::MaxEvents; ++i)
    button(Button::South, i % 2 == 0);
  const auto reset = drain(controller);
  ASSERT_EQ(reset.size(), 1u);
  EXPECT_EQ(reset[0].reason, Event::ResetReason::Overflow);
  EXPECT_EQ(controller.state().buttons, bit(Button::South));
  EXPECT_EQ(other.state().buttons, bit(Button::East));
  EXPECT_EQ(drain(other).size(), 1u);
  button(Button::South, false);
  EXPECT_EQ(drain(controller).size(), 1u);
  }

TEST_F(Controller, TriggerOverflowKeepsCurrentAnalogAndButtonSample) {
  for(size_t i = 0; i < ControllerState::MaxEvents; ++i)
    button(Button::South, i % 2 == 0);
  input->button(Button::LeftTrigger, true, .8f);
  const auto events = drain(controller);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].reason, Event::ResetReason::Overflow);
  const auto value = controller.state();
  EXPECT_EQ(value.buttons, bit(Button::LeftTrigger));
  EXPECT_EQ(value.axes[size_t(Axis::LeftTrigger)], .8f);
  input->button(Button::LeftTrigger, true, .9f);
  EXPECT_EQ(controller.state().axes[size_t(Axis::LeftTrigger)], .9f);
  EXPECT_TRUE(drain(controller).empty());
  }

TEST_F(Controller, DisconnectReplacesFullQueueWithTerminalEventAndZerosSnapshot) {
  auto copy = controller;
  input->axis(Axis::RightY, .6f);
  for(size_t i = 0; i < ControllerState::MaxEvents; ++i)
    button(Button::South, i % 2 == 0);
  input->disconnect();
  input->disconnect();
  input->setActive(false);
  button(Button::East, true);
  input->axis(Axis::RightY, 1.f);
  const auto events = drain(copy);
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].reason, Event::ResetReason::Disconnected);
  EXPECT_EQ(events[1].type, Event::Type::Disconnected);
  EXPECT_EQ(copy, controller);
  EXPECT_EQ(controller.state().buttons, 0u);
  EXPECT_EQ(controller.state().axes, GameController::State{}.axes);
  EXPECT_TRUE(drain(controller).empty());
  auto replacement = std::make_shared<ControllerState>("controller", buttons, axes, GameController::State{});
  auto reconnected = GameControllerTestAccess::make(replacement);
  EXPECT_NE(reconnected, controller);
  EXPECT_TRUE(reconnected != controller);
  EXPECT_EQ(reconnected.device().name, controller.device().name);
  }

TEST_F(Controller, ConcurrentProducerAndSnapshotReaders) {
  std::atomic_bool done = false;
  std::thread producer([&] {
    for(unsigned i = 0; i < 10000; ++i) {
      input->axis(Axis::LeftX, i % 2 == 0 ? 1.f : -1.f);
      input->button(Button::RightTrigger, i % 2 == 0, i % 2 == 0 ? .9f : 0.f);
      }
    done.store(true);
    });
  std::thread reader([&] {
    do {
      const auto value = controller.state();
      const auto stick = controller.axis(GameController::Stick::Left);
      EXPECT_TRUE(std::isfinite(stick.x));
      EXPECT_LE(std::abs(stick.x), 1.f);
      EXPECT_EQ((value.buttons & bit(Button::RightTrigger)) != 0,
                value.axes[size_t(Axis::RightTrigger)] != 0);
      } while(!done.load());
    });
  do {
    controller.pull([](const Event& event) {
      if(event.type == Event::Type::Reset)
        EXPECT_EQ(event.button, Button::None);
      else
        EXPECT_EQ(event.button, Button::RightTrigger);
      });
    } while(!done.load());
  producer.join();
  reader.join();
  EXPECT_EQ(controller.state().buttons, 0u);
  }
