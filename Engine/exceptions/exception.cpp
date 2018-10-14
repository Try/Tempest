#include "exception.h"

using namespace Tempest;

const char *SystemErrCategory::name() const noexcept {
  return "Tempest.SystemApi.Error";
  }

std::string SystemErrCategory::message(int ev) const {
  switch(static_cast<SystemErrc>(ev)){
    case SystemErrc::InvalidWindowClass:
      return "Unable to register window class";
    case SystemErrc::UnableToCreateWindow:
      return "Unable to create window";
    }
  return "(unrecognized error)";
  }

const SystemErrCategory &SystemErrCategory::instance() {
  static SystemErrCategory e;
  return e;
  }
