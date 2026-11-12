#pragma once

#include "../gamecontroller.h"

#include <vector>

namespace Tempest {

class ControllerApi final {
  public:
    static std::vector<GameController> controllers();
  };

}
