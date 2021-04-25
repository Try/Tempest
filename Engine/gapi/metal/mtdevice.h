#pragma once

#include <Tempest/AbstractGraphicsApi>
#import  <Metal/MTLPixelFormat.h>

#include "nsptr.h"

class MTLDevice;

namespace Tempest {
namespace Detail {

inline MTLPixelFormat nativeFormat(TextureFormat frm) {
  switch(frm) {
    case Undefined:
    case Last:
      return MTLPixelFormatInvalid;
    case R8:
      return MTLPixelFormatR8Unorm;
    case RG8:
      return MTLPixelFormatRG8Unorm;
    case RGB8:
      return MTLPixelFormatInvalid;
    case RGBA8:
      return MTLPixelFormatRGBA8Unorm;
    case R16:
      return MTLPixelFormatR16Unorm;
    case RG16:
      return MTLPixelFormatRG16Unorm;
    case RGB16:
      return MTLPixelFormatInvalid;
    case RGBA16:
      return MTLPixelFormatRGBA16Unorm;
    case R32F:
      return MTLPixelFormatR32Float;
    case RG32F:
      return MTLPixelFormatRG32Float;
    case RGB32F:
      return MTLPixelFormatInvalid;
    case RGBA32F:
      return MTLPixelFormatRGBA32Float;
    case Depth16:
      return MTLPixelFormatDepth16Unorm;
    case Depth24x8:
      return MTLPixelFormatInvalid;
    case Depth24S8:
      return MTLPixelFormatDepth24Unorm_Stencil8;
    case DXT1:
      return MTLPixelFormatBC1_RGBA;
    case DXT3:
      return MTLPixelFormatBC2_RGBA;
    case DXT5:
      return MTLPixelFormatBC3_RGBA;
    }
  return MTLPixelFormatInvalid;
  }

class MtDevice : public AbstractGraphicsApi::Device {
  public:
    MtDevice();
    ~MtDevice();
    void waitIdle() override;

    NsPtr impl;
    NsPtr queue;

    AbstractGraphicsApi::Props prop;
  };

}
}
