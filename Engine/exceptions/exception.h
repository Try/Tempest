#pragma once

#include <system_error>
#include <typeinfo>

namespace Tempest {

class DeviceLostException : public std::exception {
  public:
    DeviceLostException() = default;
    DeviceLostException(std::string_view message):message(std::string(message)){}
    DeviceLostException(std::string_view message, std::string log):message(std::string(message)), extLog(std::move(log)){}
    const char* what() const noexcept override;
    const char* log() const { return extLog.c_str(); }

  private:
    std::string message;
    std::string extLog;
  };

class DeviceHangException : public DeviceLostException {
  public:
  const char* what() const noexcept override { return "hit gpu timeout"; }
  };

class SwapchainSuboptimal : public std::exception {
  public:
  const char* what() const noexcept override { return "swapchain no longer matches the surface properties"; }
  };

class IncompleteFboException : public  std::exception {
  public:
  const char* what() const noexcept override { return "inconsistent framebuffer dimensions"; }
  };

class ConcurentRecordingException : public  std::exception {
  public:
  const char* what() const noexcept override { return "the command buffer is already in recording state"; }
  };

class BadTextureCastException : public  std::bad_cast {
  public:
    BadTextureCastException(std::string message):message(std::move(message)) {}
    const char* what() const noexcept override { return message.c_str(); }
  private:
    std::string message;
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
  NoDevice                     = 0,
  OutOfVideoMemory             = 1,
  OutOfHostMemory              = 2,
  InvalidShaderModule          = 3,
  UnsupportedTextureFormat     = 4,
  InvalidUniformBuffer         = 5,
  InvalidTexture               = 6,
  InvalidBufferUpdate          = 7,
  TooLargeBuffer               = 8,
  TooLargeTexture              = 9,
  InvalidStorageBuffer         = 10,
  DrawCallWithoutFbo           = 11,
  ComputeCallInRenderPass      = 12,
  UnsupportedExtension         = 13,
  InvalidAccelerationStructure = 14,
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


