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

const char *GraphicsErrCategory::name() const noexcept {
  return "Tempest.AbstractGraphicsApi.Error";
  }

std::string GraphicsErrCategory::message(int ev) const {
  switch(static_cast<GraphicsErrc>(ev)){
    case GraphicsErrc::OutOfVideoMemory:
      return "Out of device memory";
    case GraphicsErrc::OutOfHostMemory:
      return "Out of memory";
    }
  return "(unrecognized error)";
  }

const GraphicsErrCategory &GraphicsErrCategory::instance() {
  static GraphicsErrCategory e;
  return e;
  }
