#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP)
  #undef  __WINDOWS_PHONE__
  #define __WINDOWS_PHONE__ 1
#endif

#if (defined(WIN32) || defined(_WIN32)) && !defined(__WINDOWS_PHONE__)
  #undef  __WINDOWS__
  #define __WINDOWS__	1
#endif


#ifdef __APPLE__
#include <TargetConditionals.h>
#elif defined(unix) || defined(__unix__) || defined(__unix)
  #ifndef __ANDROID__
    #undef  __UNIX__
    #define __UNIX__	1
  #else
    #undef  __ANDROID__
    #define __ANDROID__	1
  #endif
#endif

#if defined(__WINDOWS_PHONE__) || defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
#undef  __MOBILE_PLATFORM__
#define __MOBILE_PLATFORM__ 1
#endif

#if defined(__APPLE__)
#ifdef __MOBILE_PLATFORM__
#define __IOS__
#else
#undef  __OSX__
#define __OSX__ 1
#endif
#endif

#endif // PLATFORM_H
