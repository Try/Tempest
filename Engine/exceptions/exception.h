#pragma once

#include <system_error>

namespace Tempest {

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
}

namespace std {
template<>
struct is_error_code_enum<Tempest::SystemErrc> : true_type {};
}


