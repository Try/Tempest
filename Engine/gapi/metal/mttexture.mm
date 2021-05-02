#include "mttexture.h"

#include <Tempest/Pixmap>
#include <Tempest/Except>
#include "mtdevice.h"

#import <Metal/MTLDevice.h>
#import <Metal/MTLCommandQueue.h>
#import <Metal/MTLCommandBuffer.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtTexture::MtTexture(MtDevice& d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm)
  :mips(mips) {
  MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
  if(desc==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

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
  [desc release];
  if(impl==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  }

MtTexture::MtTexture(MtDevice& dev, const Pixmap& pm, uint32_t mips, TextureFormat frm)
  :mips(mips) {
  MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
  if(desc==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  desc.textureType      = MTLTextureType2D;
  desc.pixelFormat      = nativeFormat(frm);
  desc.width            = pm.w();
  desc.height           = pm.h();
  desc.mipmapLevelCount = mips;

  desc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
  desc.storageMode  = MTLStorageModeShared; //TODO: MTLStorageModePrivate
  desc.usage        = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  // desc.usage |= MTLTextureUsageShaderWrite; // TODO: imageStorage

  desc.swizzle = MTLTextureSwizzleChannelsDefault; // TODO: swizzling?!

  desc.allowGPUOptimizedContents = NO;

  impl = [dev.impl newTextureWithDescriptor:desc];
  [desc release];
  if(impl==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  MTLRegion region = {
    { 0, 0, 0 },
    {pm.w(), pm.h(), 1}
    };
  [impl replaceRegion:region
          mipmapLevel:0
          withBytes:pm.data()
          bytesPerRow:pm.w()*Pixmap::bppForFormat(pm.format())
        ];

  id<MTLCommandBuffer>      cmd   = [dev.queue commandBuffer];
  id<MTLBlitCommandEncoder> enc   = [cmd blitCommandEncoder];
  [enc generateMipmapsForTexture:impl];
  [enc endEncoding];
  [cmd commit];

  // TODO: implement proper upload engine
  [cmd waitUntilCompleted];

  [enc release];
  [cmd release];
  }

Tempest::Detail::MtTexture::~MtTexture() {
  [impl release];
  }

uint32_t MtTexture::mipCount() const {
  return mips;
  }
