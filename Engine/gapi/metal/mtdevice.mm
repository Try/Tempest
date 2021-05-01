#include "mtdevice.h"

#include <Tempest/Log>

#include <Metal/MTLDevice.h>
#include <Metal/MTLCommandQueue.h>

#if __has_feature(objc_arc)
#error "Objective C++ ARC is not supported"
#endif

using namespace Tempest;
using namespace Tempest::Detail;

MtDevice::MtDevice() {
  impl  = MTLCreateSystemDefaultDevice();
  queue = [impl newCommandQueue];

  SInt32 majorVersion = 0, minorVersion = 0;
  if([[NSProcessInfo processInfo] respondsToSelector:@selector(operatingSystemVersion)]) {
    NSOperatingSystemVersion ver = [[NSProcessInfo processInfo] operatingSystemVersion];
    majorVersion = ver.majorVersion;
    minorVersion = ver.minorVersion;
    }

  // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
  static const TextureFormat smp[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RG32F, TextureFormat::RGBA32F
                                     };

  static const TextureFormat att[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RG32F, TextureFormat::RGBA32F
                                     };

  static const TextureFormat sso[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RGBA32F
                                     };

  // TODO: expose MTLPixelFormatDepth32Float - only portable depth format
  static const TextureFormat ds[]  = {};

  uint64_t smpBit = 0, attBit = 0, dsBit = 0, storBit = 0;
  for(auto& i:smp)
    smpBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:att)
    attBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:sso)
    storBit  |= uint64_t(1) << uint64_t(i);
  for(auto& i:ds)
    dsBit  |= uint64_t(1) << uint64_t(i);

  if(majorVersion>10 || (majorVersion==10 && minorVersion>=11)) {
    static const TextureFormat bc[] = {TextureFormat::DXT1, TextureFormat::DXT3, TextureFormat::DXT5};
    static const TextureFormat ds[] = {TextureFormat::Depth24S8};
    for(auto& i:bc)
      smpBit |= uint64_t(1) << uint64_t(i);
    for(auto& i:ds)
      dsBit  |= uint64_t(1) << uint64_t(i);
    }

  if(majorVersion>10 || (majorVersion==10 && minorVersion>=12)) {
    static const TextureFormat ds[] = {TextureFormat::Depth16};
    for(auto& i:ds)
      dsBit  |= uint64_t(1) << uint64_t(i);
    }

  prop.setSamplerFormats(smpBit);
  prop.setAttachFormats (attBit);
  prop.setDepthFormats  (dsBit);
  prop.setStorageFormats(storBit);
  }

MtDevice::~MtDevice() {
  [queue release];
  [impl  release];
  }

void MtDevice::waitIdle() {
  // TODO: verify, if this correct at all
  id<MTLCommandBuffer> cmd = [queue commandBuffer];
  [cmd commit];
  [cmd waitUntilCompleted];
  }

void Tempest::Detail::MtDevice::handleError(NSError *err) {
  if(err==nil)
    return;
#if !defined(NDEBUG)
  const char* e = [[err domain] UTF8String];
  Log::d("NSError: \"",e,"\"");
#endif
  throw DeviceLostException();
  }
