#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <Metal/MTLPixelFormat.h>

namespace Tempest {
namespace Detail {

class MtSwapchain;

class MtFboLayout {
  public:
    bool equals(const MtFboLayout& other) const;

    MTLPixelFormat colorFormat[MaxFramebufferAttachments] = {};
    size_t         numColors                              = 0;
    MTLPixelFormat depthFormat                            = MTLPixelFormatInvalid;
  };

}
}
