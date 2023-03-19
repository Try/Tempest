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
    case SystemErrc::UnableToSaveAsset:
      return "Unable to save asset";
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
    case GraphicsErrc::InvalidUniformBuffer:
      return "Invalid uniform buffer";
    case GraphicsErrc::InvalidTexture:
      return "Invalide texture";
    case GraphicsErrc::InvalidBufferUpdate:
      return "Invalid buffer update";
    case GraphicsErrc::TooLargeBuffer:
      return "Uniform/Storage buffer element is too large";
    case GraphicsErrc::TooLargeTexture:
      return "Texture size is too large";
    case GraphicsErrc::InvalidStorageBuffer:
      return "Invalid storage buffer";
    case GraphicsErrc::InvalidAccelerationStructure:
      return "Invalid acceleration structure";
    case GraphicsErrc::DrawCallWithoutFbo:
      return "Frame buffer is not set, before drawcall";
    case GraphicsErrc::ComputeCallInRenderPass:
      return "Dispatch compute is not allowed in render pass";
    case GraphicsErrc::UnsupportedExtension:
      return "Extension is not suported";
    }
  return "(unrecognized error)";
  }

const GraphicsErrCategory &GraphicsErrCategory::instance() {
  static GraphicsErrCategory e;
  return e;
  }


const char *SoundErrCategory::name() const noexcept {
  return "Tempest.SoundDevice.Error";
  }

std::string SoundErrCategory::message(int ev) const {
  switch(static_cast<SoundErrc>(ev)){
    case SoundErrc::NoDevice:
      return "No device";
    case SoundErrc::InvalidChannelsCount:
      return "Sound channels count must be 1 or 2";
    }
  return "(unrecognized error)";
  }

const SoundErrCategory &SoundErrCategory::instance() {
  static SoundErrCategory e;
  return e;
  }
