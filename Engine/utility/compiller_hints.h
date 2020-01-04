#pragma once

#if defined(__GNUC__) || defined(__clang__)

#define T_LIKELY(x)      __builtin_expect(!!(x), 1)
#define T_UNLIKELY(x)    __builtin_expect(!!(x), 0)

#else

#define T_LIKELY(x)      (x)
#define T_UNLIKELY(x)    (x)

#endif

#if __cplusplus>=201703L
#  if __has_cpp_attribute(nodiscard)
#  define T_NODISCARD [[nodiscard]]
#  else
#  define T_NODISCARD
#  endif
#elif defined(__GNUC__)
#  if __has_cpp_attribute(gnu::warn_unused_result)
#  define T_NODISCARD [[gnu::warn_unused_result]]
#  else
#  define T_NODISCARD
#  endif
#else
#  define T_NODISCARD
#endif

