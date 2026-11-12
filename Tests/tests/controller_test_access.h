#pragma once

#include "../../Engine/system/controller/controllerstate.h"

#include <utility>

namespace Tempest::Detail {

struct GameControllerTestAccess final {
  using Impl = GameController::Impl;

  static GameController make(std::shared_ptr<Impl> impl) {
    return GameController(std::move(impl));
    }
  };

}
