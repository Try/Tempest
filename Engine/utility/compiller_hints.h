#pragma once

#if defined(__GNUC__) || defined(__clang__)

#define T_LIKELY(x)      __builtin_expect(!!(x), 1)
#define T_UNLIKELY(x)    __builtin_expect(!!(x), 0)

#else

#define T_LIKELY(x)      (x)
#define T_UNLIKELY(x)    (x)

#endif
