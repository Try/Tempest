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
  :dev(d), mips(mips) {
  MTLTextureUsage usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  if(storageTex)
    usage |= MTLTextureUsageShaderWrite;
  impl = alloc(frm,w,h,mips,MTLStorageModePrivate,usage);
  }

MtTexture::MtTexture(MtDevice& dev, const Pixmap& pm, uint32_t mips, TextureFormat frm)
  :dev(dev), mips(mips) {
  const uint32_t smip  = (isCompressedFormat(frm) ? mips : 1);
  id<MTLTexture> stage = alloc(frm,pm.w(),pm.h(),smip,MTLStorageModeShared,MTLTextureUsageShaderRead);

  try{
    impl = alloc(frm,pm.w(),pm.h(),mips,MTLStorageModePrivate,MTLTextureUsageShaderRead);
    }
  catch(...){
    [stage release];
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
    }

  if(isCompressedFormat(frm))
    createCompressedTexture(stage,pm,frm,mips); else
    createRegularTexture(stage,pm);

  @autoreleasepool {
    id<MTLCommandBuffer>      cmd = [dev.queue commandBuffer];
    id<MTLBlitCommandEncoder> enc = [cmd blitCommandEncoder];
    if(isCompressedFormat(frm)) {
      [enc copyFromTexture:stage
                           sourceSlice:0
                           sourceLevel:0
                           toTexture:impl
                           destinationSlice:0
                           destinationLevel:0
                           sliceCount:1
                           levelCount:mips];
      } else {
      [enc copyFromTexture:stage
                           sourceSlice:0
                           sourceLevel:0
                           toTexture:impl
                           destinationSlice:0
                           destinationLevel:0
                           sliceCount:1
                           levelCount:1];
      if(mips>1)
        [enc generateMipmapsForTexture:impl];
      }

    [enc endEncoding];
    [cmd commit];
    // TODO: implement proper upload engine
    [cmd waitUntilCompleted];
    }
  [stage release];
  }

MtTexture::~MtTexture() {
  [impl release];
  }

void MtTexture::createCompressedTexture(id<MTLTexture> val, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
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
    [val replaceRegion:region
           mipmapLevel:i
           withBytes  :pdata
           bytesPerRow:pitch
          ];

    w = std::max<uint32_t>(1,w/2);
    h = std::max<uint32_t>(1,h/2);
    pdata += pitch*hBlk;
    }
  }

void MtTexture::createRegularTexture(id<MTLTexture> val, const Pixmap& p) {
  MTLRegion region = {
    { 0, 0, 0 },
    {p.w(), p.h(), 1}
    };
  [val replaceRegion:region
         mipmapLevel:0
           withBytes:p.data()
         bytesPerRow:p.w()*Pixmap::bppForFormat(p.format())
        ];
  }

id<MTLTexture> MtTexture::alloc(TextureFormat frm, const uint32_t w, const uint32_t h, const uint32 mips,
                                MTLStorageMode smode, MTLTextureUsage umode) {
  MTLTextureDescriptor* desc = [MTLTextureDescriptor new];
  if(desc==nil)
    throw std::system_error(GraphicsErrc::OutOfHostMemory);

  desc.textureType      = MTLTextureType2D;
  desc.pixelFormat      = nativeFormat(frm);
  desc.width            = w;
  desc.height           = h;
  desc.mipmapLevelCount = mips;

  desc.cpuCacheMode     = MTLCPUCacheModeWriteCombined;
  desc.storageMode      = smode;
  desc.usage            = umode;

  desc.swizzle = MTLTextureSwizzleChannelsDefault; // TODO: swizzling?!
  desc.allowGPUOptimizedContents = YES;

  auto ret = [dev.impl newTextureWithDescriptor:desc];
  [desc release];
  if(ret==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  return ret;
  }

uint32_t MtTexture::mipCount() const {
  return mips;
  }

void MtTexture::readPixels(Pixmap& out, TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip) {
  Pixmap::Format  pfrm = Pixmap::toPixmapFormat(frm);
  size_t          bpp  = Pixmap::bppForFormat(pfrm);
  if(bpp==0)
    throw std::runtime_error("not implemented");
  out = Pixmap(w,h,pfrm);

  id<MTLTexture> stage = alloc(frm,w,h,1,MTLStorageModeShared,MTLTextureUsageShaderRead);
  @autoreleasepool {
    id<MTLCommandBuffer>      cmd = [dev.queue commandBuffer];
    id<MTLBlitCommandEncoder> enc = [cmd blitCommandEncoder];
    [enc copyFromTexture:impl
                         sourceSlice:0
                         sourceLevel:mip
                         toTexture:stage
                         destinationSlice:0
                         destinationLevel:0
                         sliceCount:1
                         levelCount:1];

    [enc endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
    }

  [stage getBytes: out.data()
    bytesPerRow: w*bpp
    fromRegion : MTLRegionMake2D(0,0,w,h)
    mipmapLevel: 0
    ];
  [stage release];
  }
