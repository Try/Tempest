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
#ifdef __IOS__
  const MTLStorageMode smode = MTLStorageModeShared;
#else
  const MTLStorageMode smode = MTLStorageModeManaged;
#endif
  id<MTLTexture> stage = alloc(frm,pm.w(),pm.h(),smip,smode,MTLTextureUsageShaderRead);

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

  MTLStorageMode opt = MTLStorageModeManaged;
#ifdef __IOS__
  opt = MTLStorageModeShared;
#else
  opt = MTLStorageModeManaged;
#endif

  id<MTLTexture> stage = alloc(frm,w,h,1,opt,MTLTextureUsageShaderRead);
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
    if(opt==MTLStorageModeManaged)
      [enc synchronizeResource:stage];
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

uint32_t MtTexture::bitCount() const {
  MTLPixelFormat frm = impl.pixelFormat;
  switch(frm) {
    case MTLPixelFormatInvalid:
      return 0;
    /* Normal 8 bit formats */
    case MTLPixelFormatA8Unorm:
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatR8Unorm_sRGB:
    case MTLPixelFormatR8Snorm:
    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
      return 8;
    /* Normal 16 bit formats */
    case MTLPixelFormatR16Unorm:
    case MTLPixelFormatR16Snorm:
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatRG8Unorm:
    case MTLPixelFormatRG8Unorm_sRGB:
    case MTLPixelFormatRG8Snorm:
    case MTLPixelFormatRG8Uint:
    case MTLPixelFormatRG8Sint:
      return 16;
    /* Packed 16 bit formats */
    case MTLPixelFormatB5G6R5Unorm:
    case MTLPixelFormatA1BGR5Unorm:
    case MTLPixelFormatABGR4Unorm:
    case MTLPixelFormatBGR5A1Unorm:
      return 16;
    /* Normal 32 bit formats */
    case MTLPixelFormatR32Uint:
    case MTLPixelFormatR32Sint:
    case MTLPixelFormatR32Float:
    case MTLPixelFormatRG16Unorm:
    case MTLPixelFormatRG16Snorm:
    case MTLPixelFormatRG16Uint:
    case MTLPixelFormatRG16Sint:
    case MTLPixelFormatRG16Float:
    case MTLPixelFormatRGBA8Unorm:
    case MTLPixelFormatRGBA8Unorm_sRGB:
    case MTLPixelFormatRGBA8Snorm:
    case MTLPixelFormatRGBA8Uint:
    case MTLPixelFormatRGBA8Sint:
    case MTLPixelFormatBGRA8Unorm:
    case MTLPixelFormatBGRA8Unorm_sRGB:
      return 32;
    /* Packed 32 bit formats */
    case MTLPixelFormatRGB10A2Unorm:
    case MTLPixelFormatRGB10A2Uint:
    case MTLPixelFormatRG11B10Float:
    case MTLPixelFormatRGB9E5Float:
    case MTLPixelFormatBGR10A2Unorm:
    case MTLPixelFormatBGR10_XR:
    case MTLPixelFormatBGR10_XR_sRGB:
      return 32;
    /* Normal 64 bit formats */
    case MTLPixelFormatRG32Uint:
    case MTLPixelFormatRG32Sint:
    case MTLPixelFormatRG32Float:
    case MTLPixelFormatRGBA16Unorm:
    case MTLPixelFormatRGBA16Snorm:
    case MTLPixelFormatRGBA16Uint:
    case MTLPixelFormatRGBA16Sint:
    case MTLPixelFormatRGBA16Float:
      return 64;
    case MTLPixelFormatBGRA10_XR:
    case MTLPixelFormatBGRA10_XR_sRGB:
      return 64;
    /* Normal 128 bit formats */
    case MTLPixelFormatRGBA32Uint:
    case MTLPixelFormatRGBA32Sint:
    case MTLPixelFormatRGBA32Float:
      return 128;

    /* Compressed formats. */
    /* S3TC/DXT */
    case MTLPixelFormatBC1_RGBA:
    case MTLPixelFormatBC1_RGBA_sRGB:
      return 4;
    case MTLPixelFormatBC2_RGBA:
    case MTLPixelFormatBC2_RGBA_sRGB:
    case MTLPixelFormatBC3_RGBA:
    case MTLPixelFormatBC3_RGBA_sRGB:
      return 8;
    /* RGTC */
    case MTLPixelFormatBC4_RUnorm:
    case MTLPixelFormatBC4_RSnorm:
    case MTLPixelFormatBC5_RGUnorm:
    case MTLPixelFormatBC5_RGSnorm:
      return 8;

    /* BPTC */
    case MTLPixelFormatBC6H_RGBFloat:
    case MTLPixelFormatBC6H_RGBUfloat:
    case MTLPixelFormatBC7_RGBAUnorm:
    case MTLPixelFormatBC7_RGBAUnorm_sRGB:
      return 8;

    /* PVRTC */
    case MTLPixelFormatPVRTC_RGB_2BPP:
    case MTLPixelFormatPVRTC_RGB_2BPP_sRGB:
    case MTLPixelFormatPVRTC_RGB_4BPP:
    case MTLPixelFormatPVRTC_RGB_4BPP_sRGB:
    case MTLPixelFormatPVRTC_RGBA_2BPP:
    case MTLPixelFormatPVRTC_RGBA_2BPP_sRGB:
    case MTLPixelFormatPVRTC_RGBA_4BPP:
    case MTLPixelFormatPVRTC_RGBA_4BPP_sRGB:
      return 0;
    /* ETC2 */
    case MTLPixelFormatEAC_R11Unorm:
    case MTLPixelFormatEAC_R11Snorm:
    case MTLPixelFormatEAC_RG11Unorm:
    case MTLPixelFormatEAC_RG11Snorm:
    case MTLPixelFormatEAC_RGBA8:
    case MTLPixelFormatEAC_RGBA8_sRGB:
      return 0;
    case MTLPixelFormatETC2_RGB8:
    case MTLPixelFormatETC2_RGB8_sRGB:
    case MTLPixelFormatETC2_RGB8A1:
    case MTLPixelFormatETC2_RGB8A1_sRGB:
      return 0;
    /* ASTC */
    case MTLPixelFormatASTC_4x4_sRGB:
    case MTLPixelFormatASTC_5x4_sRGB:
    case MTLPixelFormatASTC_5x5_sRGB:
    case MTLPixelFormatASTC_6x5_sRGB:
    case MTLPixelFormatASTC_6x6_sRGB:
    case MTLPixelFormatASTC_8x5_sRGB:
    case MTLPixelFormatASTC_8x6_sRGB:
    case MTLPixelFormatASTC_8x8_sRGB:
    case MTLPixelFormatASTC_10x5_sRGB:
    case MTLPixelFormatASTC_10x6_sRGB:
    case MTLPixelFormatASTC_10x8_sRGB:
    case MTLPixelFormatASTC_10x10_sRGB:
    case MTLPixelFormatASTC_12x10_sRGB:
    case MTLPixelFormatASTC_12x12_sRGB:
      return 0;
    case MTLPixelFormatASTC_4x4_LDR:
    case MTLPixelFormatASTC_5x4_LDR:
    case MTLPixelFormatASTC_5x5_LDR:
    case MTLPixelFormatASTC_6x5_LDR:
    case MTLPixelFormatASTC_6x6_LDR:
    case MTLPixelFormatASTC_8x5_LDR:
    case MTLPixelFormatASTC_8x6_LDR:
    case MTLPixelFormatASTC_8x8_LDR:
    case MTLPixelFormatASTC_10x5_LDR:
    case MTLPixelFormatASTC_10x6_LDR:
    case MTLPixelFormatASTC_10x8_LDR:
    case MTLPixelFormatASTC_10x10_LDR:
    case MTLPixelFormatASTC_12x10_LDR:
    case MTLPixelFormatASTC_12x12_LDR:
      return 0;
    // ASTC HDR (High Dynamic Range) Formats
    case MTLPixelFormatASTC_4x4_HDR:
    case MTLPixelFormatASTC_5x4_HDR:
    case MTLPixelFormatASTC_5x5_HDR:
    case MTLPixelFormatASTC_6x5_HDR:
    case MTLPixelFormatASTC_6x6_HDR:
    case MTLPixelFormatASTC_8x5_HDR:
    case MTLPixelFormatASTC_8x6_HDR:
    case MTLPixelFormatASTC_8x8_HDR:
    case MTLPixelFormatASTC_10x5_HDR:
    case MTLPixelFormatASTC_10x6_HDR:
    case MTLPixelFormatASTC_10x8_HDR:
    case MTLPixelFormatASTC_10x10_HDR:
    case MTLPixelFormatASTC_12x10_HDR:
    case MTLPixelFormatASTC_12x12_HDR:
    case MTLPixelFormatGBGR422:
    case MTLPixelFormatBGRG422:
      return 0;
    /* Depth */
    case MTLPixelFormatDepth16Unorm:
      return 16;
    case MTLPixelFormatDepth32Float:
      return 32;
    /* Stencil */
    case MTLPixelFormatStencil8:
      return 8;
    /* Depth Stencil */
    case MTLPixelFormatDepth24Unorm_Stencil8:
      return 32;
    case MTLPixelFormatDepth32Float_Stencil8:
      return 40;
    case MTLPixelFormatX32_Stencil8:
      return 40;
    case MTLPixelFormatX24_Stencil8:
      return 32;
    }
  return 0;
  }
