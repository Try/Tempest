#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <Metal/MTLPixelFormat.h>

namespace Tempest {
namespace Detail {

class MtSwapchain;

class MtFboLayout : public AbstractGraphicsApi::FboLayout {
  public:
    MtFboLayout(MtSwapchain** sw, TextureFormat* att, size_t attCount);

    bool equals(const FboLayout& other) const override;

    MTLPixelFormat colorFormat[256] = {};
    size_t         numColors        = 0;
    MTLPixelFormat depthFormat      = MTLPixelFormatInvalid;
  };

}
}
