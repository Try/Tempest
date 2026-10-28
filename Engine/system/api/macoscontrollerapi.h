#pragma once

#include "../controllerstate.h"

#include <memory>

namespace Tempest::Detail {

// Native registry and handlers belong to the main thread. Handles only retain
// ControllerState, so releasing their last copy never tears down AppKit objects.
class MacOSControllerApi final {
  public:
    MacOSControllerApi();
    ~MacOSControllerApi();
    MacOSControllerApi(const MacOSControllerApi&) = delete;
    MacOSControllerApi& operator=(const MacOSControllerApi&) = delete;

    static void requireMainThread();
    std::vector<std::shared_ptr<ControllerState>> controllers() const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
  };

}
