#include "mttexture.h"

#include <Tempest/Pixmap>
#include <Tempest/Except>
#include "mtdevice.h"

#include <Tempest/AbstractGraphicsApi>

#import <Metal/MTLDevice.h>
#import <Metal/MTLCommandQueue.h>
#import <Metal/MTLCommandBuffer.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtTexture::MtTexture(MtDevice& d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm, bool storageTex)
  :mips(mips) {
  MTLTextureDescriptor* desc = [MTLTextureDescriptor new];
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

  if(storageTex)
    desc.usage |= MTLTextureUsageShaderWrite;

  desc.swizzle = MTLTextureSwizzleChannelsDefault; // TODO: swizzling?!

  desc.allowGPUOptimizedContents = NO;

  impl = [d.impl newTextureWithDescriptor:desc];
  [desc release];
  if(impl==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  }

MtTexture::MtTexture(MtDevice& dev, const Pixmap& pm, uint32_t mips, TextureFormat frm)
  :mips(mips) {
  MTLTextureDescriptor* desc = [MTLTextureDescriptor new];
  if(desc==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  desc.textureType      = MTLTextureType2D;
  desc.pixelFormat      = nativeFormat(frm);
  desc.width            = pm.w();
  desc.height           = pm.h();
  desc.mipmapLevelCount = mips;

  desc.cpuCacheMode = MTLCPUCacheModeWriteCombined;
  desc.storageMode  = MTLStorageModeShared; //TODO: MTLStorageModePrivate
  desc.usage        = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;

  desc.swizzle = MTLTextureSwizzleChannelsDefault; // TODO: swizzling?!

  desc.allowGPUOptimizedContents = YES;

  impl = [dev.impl newTextureWithDescriptor:desc];
  [desc release];
  if(impl==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  @autoreleasepool {
    if(isCompressedFormat(frm)) {
      createCompressedTexture(pm,frm,mips);
      return;
      }

    MTLRegion region = {
      { 0, 0, 0 },
      {pm.w(), pm.h(), 1}
      };
    [impl replaceRegion:region
            mipmapLevel:0
            withBytes:pm.data()
            bytesPerRow:pm.w()*Pixmap::bppForFormat(pm.format())
          ];

    if(mips>1) {
      id<MTLCommandBuffer>      cmd   = [dev.queue commandBuffer];
      id<MTLBlitCommandEncoder> enc   = [cmd blitCommandEncoder];
      [enc generateMipmapsForTexture:impl];
      [enc endEncoding];
      [cmd commit];

      // TODO: implement proper upload engine
      [cmd waitUntilCompleted];
      }
    }
  }

MtTexture::~MtTexture() {
  [impl release];
  }

void MtTexture::createCompressedTexture(const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
  uint32_t       blockSize = (frm==TextureFormat::DXT1) ? 8 : 16;
  const uint8_t* pdata     = reinterpret_cast<const uint8_t*>(p.data());

  uint32_t w = p.w(), h = p.h();
  for(uint32_t i=0; i<mipCnt; i++) {
    uint32_t wBlk  = (w+3)/4;
    uint32_t hBlk  = (h+3)/4;
    uint32_t pitch = wBlk*blockSize;

    MTLRegion region = {
      { 0, 0, 0 },
      { w, h, 1 }
      };
    [impl replaceRegion:region
            mipmapLevel:i
            withBytes  :pdata
            bytesPerRow:pitch
          ];

    w = std::max<uint32_t>(1,w/2);
    h = std::max<uint32_t>(1,h/2);
    pdata += pitch*hBlk;
    }
  }

uint32_t MtTexture::mipCount() const {
  return mips;
  }
