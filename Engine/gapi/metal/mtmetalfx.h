#pragma once

#if defined(__APPLE__)
#include <Availability.h>
#include <TargetConditionals.h>

#if __has_include(<MetalFX/MTLFXTemporalScaler.h>)
#if TARGET_OS_IPHONE
#if defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 160000
#define TEMPEST_METALFX_TEMPORAL_SDK_AVAILABLE
#endif
#elif defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 130000
#define TEMPEST_METALFX_TEMPORAL_SDK_AVAILABLE
#endif
#endif

#endif

#if defined(TEMPEST_BUILD_METALFX) && defined(TEMPEST_METALFX_TEMPORAL_SDK_AVAILABLE)
#define TEMPEST_BUILD_METALFX_TEMPORAL
#endif
