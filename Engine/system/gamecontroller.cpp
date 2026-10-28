#include "gamecontroller.h"

#include "api/controllerapi.h"
#include "controllerstate.h"

#include <utility>

using namespace Tempest;

GameController::GameController(std::shared_ptr<Detail::ControllerState> impl) : impl(std::move(impl)) {
  }

std::vector<GameController> GameController::controllers() {
  return Detail::ControllerApi::controllers();
  }

const GameController::Device& GameController::device() const {
  return impl->device();
  }

GameController::State GameController::state() const {
  return impl->state();
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
  lifetime->pull(callback);
  }
