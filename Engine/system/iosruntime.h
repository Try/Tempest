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

// Request an adaptive display-link range. Values are normalized to
// minimum <= preferred <= maximum. A zero maximum restores the system
// default cadence.
void setPreferredFrameRateRange(uint32_t minimumFramesPerSecond,
                                uint32_t maximumFramesPerSecond,
                                uint32_t preferredFramesPerSecond);

// Control the iOS idle timer. The preference survives scene deactivation and
// is applied again when the scene becomes active.
void setIdleTimerDisabled(bool disabled);

}

#endif
