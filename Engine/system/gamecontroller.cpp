#include "gamecontroller.h"

#include "controller/controllerstate.h"

#include <utility>

using namespace Tempest;

GameController::GameController(std::shared_ptr<Impl> impl) : impl(std::move(impl)) {
  }

const GameController::Device& GameController::device() const {
  static const Device empty;
  return impl ? impl->device() : empty;
  }

GameController::State GameController::state() const {
  return impl ? impl->state() : State{};
  }

Vec2 GameController::axis(Stick stick) const {
  const auto value = state();
  switch(stick) {
    case Stick::Left:  return {value.axes[size_t(Axis::LeftX)], value.axes[size_t(Axis::LeftY)]};
    case Stick::Right: return {value.axes[size_t(Axis::RightX)], value.axes[size_t(Axis::RightY)]};
    }
  return {};
  }

void GameController::pull(const std::function<void(const Event&)>& callback) const {
  // A callback may destroy the handle through which pull was called.
  auto lifetime = impl;
  if(lifetime)
    lifetime->pull(callback);
  }
