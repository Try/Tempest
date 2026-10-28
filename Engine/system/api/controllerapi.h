#pragma once

#include "../gamecontroller.h"

namespace Tempest::Detail {

class ControllerApi final {
  public:
    static std::vector<GameController> controllers();
    static GameController controller(std::shared_ptr<ControllerState> state);
  };

}
