#include "mttexture.h"

#include "mtdevice.h"

#include <Metal/MTLDevice.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtTexture::MtTexture(MtDevice& d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm)
  :mips(mips) {
  MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];

  desc.textureType      = MTLTextureType2D;
  desc.pixelFormat      = nativeFormat(frm);
  desc.width            = w;
  desc.height           = h;
  desc.mipmapLevelCount = mips;

  desc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
  desc.storageMode  = MTLStorageModeShared; //TODO: MTLStorageModePrivate
  desc.usage        = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  // desc.usage |= MTLTextureUsageShaderWrite; // TODO: imageStorage

  desc.swizzle = MTLTextureSwizzleChannelsDefault; // TODO: swizzling?!

  desc.allowGPUOptimizedContents = NO;

  impl = [d.impl newTextureWithDescriptor:desc];
  }

uint32_t MtTexture::mipCount() const {
  return mips;
  }

Tempest::Detail::MtTexture::~MtTexture() {
  [impl release];
  }
