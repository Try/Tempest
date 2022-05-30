#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/Metal.hpp>

namespace Tempest {
namespace Detail {

class MtSwapchain;

class MtFboLayout {
  public:
    bool equals(const MtFboLayout& other) const;

    MTL::PixelFormat colorFormat[MaxFramebufferAttachments] = {};
    size_t           numColors                              = 0;
    MTL::PixelFormat depthFormat                            = MTL::PixelFormatInvalid;
  };

}
}
