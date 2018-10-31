#pragma once

#include <system_error>

namespace Tempest {

class DeviceLostException : std::exception{
  const char* what() const noexcept override { return "device is lost"; }
  };

enum class SystemErrc {
  InvalidWindowClass   = 0,
  UnableToCreateWindow = 1
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
  NoDevice            = 0,
  OutOfVideoMemory    = 1,
  OutOfHostMemory     = 2,
  InvalidShaderModule = 3
  };

struct GraphicsErrCategory : std::error_category {
  const char* name() const noexcept override;
  std::string message(int ev) const override;

  static const GraphicsErrCategory& instance();
  };

inline std::error_code make_error_code(Tempest::GraphicsErrc e) noexcept {
  return {static_cast<int>(e),Tempest::GraphicsErrCategory::instance()};
  }
}

namespace std {
template<>
struct is_error_code_enum<Tempest::SystemErrc> : true_type {};

template<>
struct is_error_code_enum<Tempest::GraphicsErrc> : true_type {};
}


