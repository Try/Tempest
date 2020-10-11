#pragma once

#include <system_error>

namespace Tempest {

class DeviceLostException : std::exception {
  const char* what() const noexcept override { return "device is lost"; }
  };

class IncompleteFboException : std::exception {
  const char* what() const noexcept override { return "inconsistent framebuffer dimensions"; }
  };

class ConcurentRecordingException : std::exception {
  const char* what() const noexcept override { return "the command buffer is already in recording state"; }
  };

enum class SystemErrc {
  InvalidWindowClass   = 0,
  UnableToCreateWindow = 1,
  UnableToOpenFile     = 2,
  UnableToLoadAsset    = 3,
  UnableToSaveAsset    = 4,
  };

struct SystemErrCategory : std::error_category {
  const char* name() const noexcept override;
  std::string message(int ev) const override;

  static const SystemErrCategory& instance();
  };

inline std::error_code make_error_code(Tempest::SystemErrc e) noexcept {
  return {static_cast<int>(e),Tempest::SystemErrCategory::instance()};
  }


enum class GraphicsErrc {
  NoDevice                  = 0,
  OutOfVideoMemory          = 1,
  OutOfHostMemory           = 2,
  InvalidShaderModule       = 3,
  UnsupportedTextureFormat  = 4,
  InvalidUniformBuffer      = 5,
  InvalidTexture            = 6,
  InvalidBufferUpdate       = 7,
  TooLardgeUbo              = 8,
  InvalidStorageBuffer      = 9,
  DrawCallWithoutFbo        = 10,
  ComputeCallInRenderPass   = 11,
  };

struct GraphicsErrCategory : std::error_category {
  const char* name() const noexcept override;
  std::string message(int ev) const override;

  static const GraphicsErrCategory& instance();
  };

inline std::error_code make_error_code(Tempest::GraphicsErrc e) noexcept {
  return {static_cast<int>(e),Tempest::GraphicsErrCategory::instance()};
  }

enum class SoundErrc {
  NoDevice             = 0,
  InvalidChannelsCount = 1
  };

struct SoundErrCategory : std::error_category {
  const char* name() const noexcept override;
  std::string message(int ev) const override;

  static const SoundErrCategory& instance();
  };

inline std::error_code make_error_code(Tempest::SoundErrc e) noexcept {
  return {static_cast<int>(e),Tempest::SoundErrCategory::instance()};
  }
}

namespace std {
template<>
struct is_error_code_enum<Tempest::SystemErrc> : true_type {};

template<>
struct is_error_code_enum<Tempest::GraphicsErrc> : true_type {};

template<>
struct is_error_code_enum<Tempest::SoundErrc> : true_type {};
}


