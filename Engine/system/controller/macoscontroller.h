#pragma once

#include "controllerstate.h"

#ifdef __OBJC__
@class GCController;
#else
class GCController;
#endif

namespace Tempest::Detail {

class MacosControllerApi;

class MacosController final : public GameController::Impl {
  private:
    MacosController(GCController* controller, bool active);
    void bind(const std::shared_ptr<MacosController>& self);
    void disconnect();
    void setActive(bool active);
    void feedSample();

    // The registry retains and clears this pointer on the main thread.
    GCController* native;

  friend class MacosControllerApi;
  };

}
