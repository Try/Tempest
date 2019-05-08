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
    case SystemErrc::UnableToOpenFile:
      return "Unable to open file";
    case SystemErrc::UnableToLoadAsset:
      return "Unable to load asset";
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
    case GraphicsErrc::NoDevice:
      return "No device";
    case GraphicsErrc::OutOfVideoMemory:
      return "Out of device memory";
    case GraphicsErrc::OutOfHostMemory:
      return "Out of memory";
    case GraphicsErrc::InvalidShaderModule:
      return "Invalid shader module";
    case GraphicsErrc::UnsupportedTextureFormat:
      return "Usage of texture format whitch is not supported by device";
    }
  return "(unrecognized error)";
  }

const GraphicsErrCategory &GraphicsErrCategory::instance() {
  static GraphicsErrCategory e;
  return e;
  }
