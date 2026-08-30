#pragma once

#include "platform.h"

#include <cstdint>

#if defined(__IOS__)

namespace Tempest::iOS {

// Temporarily return control to UIKit while running on Tempest's iOS engine
// fiber. Calls from worker threads or before the native window exists are
// ignored.
void yieldToUIKit();

// Request a fixed display-link cadence. Zero restores the native/default
// cadence selected by the system.
void setPreferredFrameRate(uint32_t framesPerSecond);

}

#endif
