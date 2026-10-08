#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#if defined(TEMPEST_BUILD_METALFX)
#define MTLFX_PRIVATE_IMPLEMENTATION
#endif
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#if defined(TEMPEST_BUILD_METALFX)
#include <MetalFX/MetalFX.hpp>
#endif
