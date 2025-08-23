#if defined(TEMPEST_BUILD_METAL)

#include "mttexture.h"

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Pixmap>
#include <Tempest/Except>

#include "mtdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

static MTL::TextureSwizzle swizzle(ComponentSwizzle cs, MTL::TextureSwizzle def){
  switch(cs) {
    case ComponentSwizzle::Identity:
      return def;
    case ComponentSwizzle::R:
      return MTL::TextureSwizzleRed;
    case ComponentSwizzle::G:
      return MTL::TextureSwizzleGreen;
    case ComponentSwizzle::B:
      return MTL::TextureSwizzleBlue;
    case ComponentSwizzle::A:
      return MTL::TextureSwizzleAlpha;
    }
  return def;
  }

static uint32_t alignedSize(uint32_t w, uint32_t h, uint32_t a) {
  return ((w*sizeof(uint32_t)+a-1) & ~(a-1)) * (h);
  }

MtTexture::MtTexture(MtDevice& d, const uint32_t w, const uint32_t h, const uint32_t depth,
                     uint32_t mipCnt, TextureFormat frm, bool storageTex)
  :dev(d), mipCnt(mipCnt) {
  MTL::TextureUsage usage = MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead;
  if(storageTex)
    usage |= MTL::TextureUsageShaderWrite;
  if(d.prop.hasAtomicFormat(frm) && d.useNativeImageAtomic())
    usage |= MTL::TextureUsageShaderAtomic;
  impl = alloc(frm,w,h,depth,mipCnt,MTL::StorageModePrivate,usage);
  }

MtTexture::MtTexture(MtDevice& dev, const Pixmap& pm, uint32_t mipCnt, TextureFormat frm)
  :dev(dev), mipCnt(mipCnt) {
  const uint32_t smip  = (isCompressedFormat(frm) ? mipCnt : 1);
#ifdef __IOS__
  const MTL::StorageMode smode = MTL::StorageModeShared;
#else
  const MTL::StorageMode smode = MTL::StorageModeManaged;
#endif
  NsPtr<MTL::Texture> stage = alloc(frm,pm.w(),pm.h(),1,smip,smode,MTL::TextureUsageShaderRead);
  impl = alloc(frm,pm.w(),pm.h(),1,mipCnt,MTL::StorageModePrivate,MTL::TextureUsageShaderRead);

  if(isCompressedFormat(frm))
    createCompressedTexture(*stage,pm,frm,mipCnt); else
    createRegularTexture(*stage,pm);

  auto pool = NsPtr<NS::AutoreleasePool>::init();
  auto cmd  = dev.queue->commandBuffer();
  auto enc  = cmd->blitCommandEncoder();
  if(isCompressedFormat(frm)) {
    enc->copyFromTexture(stage.get(),0,0, impl.get(),0,0, 1,mipCnt);
    } else {
    enc->copyFromTexture(stage.get(),0,0, impl.get(),0,0, 1,1);
    if(mipCnt>1)
      enc->generateMipmaps(impl.get());
    }
  enc->endEncoding();
  cmd->commit();
  // TODO: implement proper upload engine
  cmd->waitUntilCompleted();
  }

MtTexture::~MtTexture() {
  }

void MtTexture::createCompressedTexture(MTL::Texture& val, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
  uint32_t       blockSize = (frm==TextureFormat::DXT1) ? 8 : 16;
  const uint8_t* pdata     = reinterpret_cast<const uint8_t*>(p.data());

  uint32_t w = p.w(), h = p.h();
  for(uint32_t i=0; i<mipCnt; i++) {
    uint32_t wBlk  = (w+3)/4;
    uint32_t hBlk  = (h+3)/4;
    uint32_t pitch = wBlk*blockSize;

    val.replaceRegion(MTL::Region(0,0,w,h),i,pdata,pitch);

    w = std::max<uint32_t>(1,w/2);
    h = std::max<uint32_t>(1,h/2);
    pdata += pitch*hBlk;
    }
  }

void MtTexture::createRegularTexture(MTL::Texture& val, const Pixmap& p) {
  val.replaceRegion(MTL::Region(0,0,p.w(),p.h()), 0, p.data(), p.w()*Pixmap::bppForFormat(p.format()));
  }

NsPtr<MTL::Texture> MtTexture::alloc(TextureFormat frm,
                                     const uint32_t w, const uint32_t h, const uint32_t d, const uint32_t mips,
                                     MTL::StorageMode smode, MTL::TextureUsage umode) {
  auto desc = NsPtr<MTL::TextureDescriptor>::init();
  if(desc==nullptr)
    throw std::system_error(GraphicsErrc::OutOfHostMemory);

  const bool linear = ((umode & MTL::TextureUsageShaderWrite) && dev.prop.hasAtomicFormat(frm) && !dev.useNativeImageAtomic());
  if(linear) {
    const uint32_t align   = dev.linearImageAlignment();
    const uint32_t memSize = alignedSize(w,h,align);
    linearMem = NsPtr<MTL::Buffer>(dev.impl->newBuffer(memSize,MTL::ResourceStorageModePrivate));
    if(linearMem==nullptr)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);
    }

  desc->setTextureType(d==1 ? MTL::TextureType2D : MTL::TextureType3D);
  desc->setPixelFormat(nativeFormat(frm));
  desc->setWidth(w);
  desc->setHeight(h);
  desc->setDepth(d);
  desc->setMipmapLevelCount(mips);
  desc->setCpuCacheMode(MTL::CPUCacheModeDefaultCache);
  desc->setStorageMode(smode);
  desc->setUsage(umode);
  desc->setAllowGPUOptimizedContents(true);

  MTL::TextureSwizzleChannels sw;
  sw.red   = MTL::TextureSwizzleRed;
  sw.green = MTL::TextureSwizzleGreen;
  sw.blue  = MTL::TextureSwizzleBlue;
  sw.alpha = MTL::TextureSwizzleAlpha;
  desc->setSwizzle(sw);

  NsPtr<MTL::Texture> ret;
  if(linear) {
    const uint32_t align = dev.linearImageAlignment();
    const uint32_t bpr   = alignedSize(w,1,align);
    ret = NsPtr<MTL::Texture>(linearMem->newTexture(desc.get(), 0, bpr));
    } else {
    ret = NsPtr<MTL::Texture>(dev.impl->newTexture(desc.get()));
    }
  if(ret==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  return ret;
  }

uint32_t MtTexture::mipCount() const {
  return mipCnt;
  }

void MtTexture::readPixels(Pixmap& out, TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip) {
  size_t bpp = Pixmap::bppForFormat(frm);
  if(bpp==0)
    throw std::runtime_error("not implemented");
  out = Pixmap(w,h,frm);

  MTL::StorageMode opt = MTL::StorageModeManaged;
#ifdef __IOS__
  opt = MTL::StorageModeShared;
#else
  opt = MTL::StorageModeManaged;
#endif

  NsPtr<MTL::Texture> stage = alloc(frm,w,h,1,1,opt,MTL::TextureUsageShaderRead);
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  auto cmd  = dev.queue->commandBuffer();
  auto enc  = cmd->blitCommandEncoder();
  enc->copyFromTexture(impl.get(),0,mip,MTL::Origin(),
                       MTL::Size(w,h,1),
                       stage.get(),0,0,MTL::Origin());
  if(opt==MTL::StorageModeManaged)
    enc->synchronizeResource(stage.get());
  enc->endEncoding();
  cmd->commit();
  cmd->waitUntilCompleted();

  stage->getBytes(out.data(), w*bpp,MTL::Region(0,0,w,h),0);
  }

uint32_t MtTexture::bitCount() {
  MTL::PixelFormat frm = impl->pixelFormat();
  switch(frm) {
    case MTL::PixelFormatInvalid:
      return 0;
    /* Normal 8 bit formats */
    case MTL::PixelFormatA8Unorm:
    case MTL::PixelFormatR8Unorm:
    case MTL::PixelFormatR8Unorm_sRGB:
    case MTL::PixelFormatR8Snorm:
    case MTL::PixelFormatR8Uint:
    case MTL::PixelFormatR8Sint:
      return 8;
    /* Normal 16 bit formats */
    case MTL::PixelFormatR16Unorm:
    case MTL::PixelFormatR16Snorm:
    case MTL::PixelFormatR16Uint:
    case MTL::PixelFormatR16Sint:
    case MTL::PixelFormatR16Float:
    case MTL::PixelFormatRG8Unorm:
    case MTL::PixelFormatRG8Unorm_sRGB:
    case MTL::PixelFormatRG8Snorm:
    case MTL::PixelFormatRG8Uint:
    case MTL::PixelFormatRG8Sint:
      return 16;
    /* Packed 16 bit formats */
    case MTL::PixelFormatB5G6R5Unorm:
    case MTL::PixelFormatA1BGR5Unorm:
    case MTL::PixelFormatABGR4Unorm:
    case MTL::PixelFormatBGR5A1Unorm:
      return 16;
    /* Normal 32 bit formats */
    case MTL::PixelFormatR32Uint:
    case MTL::PixelFormatR32Sint:
    case MTL::PixelFormatR32Float:
    case MTL::PixelFormatRG16Unorm:
    case MTL::PixelFormatRG16Snorm:
    case MTL::PixelFormatRG16Uint:
    case MTL::PixelFormatRG16Sint:
    case MTL::PixelFormatRG16Float:
    case MTL::PixelFormatRGBA8Unorm:
    case MTL::PixelFormatRGBA8Unorm_sRGB:
    case MTL::PixelFormatRGBA8Snorm:
    case MTL::PixelFormatRGBA8Uint:
    case MTL::PixelFormatRGBA8Sint:
    case MTL::PixelFormatBGRA8Unorm:
    case MTL::PixelFormatBGRA8Unorm_sRGB:
      return 32;
    /* Packed 32 bit formats */
    case MTL::PixelFormatRGB10A2Unorm:
    case MTL::PixelFormatRGB10A2Uint:
    case MTL::PixelFormatRG11B10Float:
    case MTL::PixelFormatRGB9E5Float:
    case MTL::PixelFormatBGR10A2Unorm:
    case MTL::PixelFormatBGR10_XR:
    case MTL::PixelFormatBGR10_XR_sRGB:
      return 32;
    /* Normal 64 bit formats */
    case MTL::PixelFormatRG32Uint:
    case MTL::PixelFormatRG32Sint:
    case MTL::PixelFormatRG32Float:
    case MTL::PixelFormatRGBA16Unorm:
    case MTL::PixelFormatRGBA16Snorm:
    case MTL::PixelFormatRGBA16Uint:
    case MTL::PixelFormatRGBA16Sint:
    case MTL::PixelFormatRGBA16Float:
      return 64;
    case MTL::PixelFormatBGRA10_XR:
    case MTL::PixelFormatBGRA10_XR_sRGB:
      return 64;
    /* Normal 128 bit formats */
    case MTL::PixelFormatRGBA32Uint:
    case MTL::PixelFormatRGBA32Sint:
    case MTL::PixelFormatRGBA32Float:
      return 128;

    /* Compressed formats. */
    /* S3TC/DXT */
    case MTL::PixelFormatBC1_RGBA:
    case MTL::PixelFormatBC1_RGBA_sRGB:
      return 4;
    case MTL::PixelFormatBC2_RGBA:
    case MTL::PixelFormatBC2_RGBA_sRGB:
    case MTL::PixelFormatBC3_RGBA:
    case MTL::PixelFormatBC3_RGBA_sRGB:
      return 8;
    /* RGTC */
    case MTL::PixelFormatBC4_RUnorm:
    case MTL::PixelFormatBC4_RSnorm:
    case MTL::PixelFormatBC5_RGUnorm:
    case MTL::PixelFormatBC5_RGSnorm:
      return 8;

    /* BPTC */
    case MTL::PixelFormatBC6H_RGBFloat:
    case MTL::PixelFormatBC6H_RGBUfloat:
    case MTL::PixelFormatBC7_RGBAUnorm:
    case MTL::PixelFormatBC7_RGBAUnorm_sRGB:
      return 8;

    /* PVRTC */
    case MTL::PixelFormatPVRTC_RGB_2BPP:
    case MTL::PixelFormatPVRTC_RGB_2BPP_sRGB:
    case MTL::PixelFormatPVRTC_RGB_4BPP:
    case MTL::PixelFormatPVRTC_RGB_4BPP_sRGB:
    case MTL::PixelFormatPVRTC_RGBA_2BPP:
    case MTL::PixelFormatPVRTC_RGBA_2BPP_sRGB:
    case MTL::PixelFormatPVRTC_RGBA_4BPP:
    case MTL::PixelFormatPVRTC_RGBA_4BPP_sRGB:
      return 0;
    /* ETC2 */
    case MTL::PixelFormatEAC_R11Unorm:
    case MTL::PixelFormatEAC_R11Snorm:
    case MTL::PixelFormatEAC_RG11Unorm:
    case MTL::PixelFormatEAC_RG11Snorm:
    case MTL::PixelFormatEAC_RGBA8:
    case MTL::PixelFormatEAC_RGBA8_sRGB:
      return 0;
    case MTL::PixelFormatETC2_RGB8:
    case MTL::PixelFormatETC2_RGB8_sRGB:
    case MTL::PixelFormatETC2_RGB8A1:
    case MTL::PixelFormatETC2_RGB8A1_sRGB:
      return 0;
    /* ASTC */
    case MTL::PixelFormatASTC_4x4_sRGB:
    case MTL::PixelFormatASTC_5x4_sRGB:
    case MTL::PixelFormatASTC_5x5_sRGB:
    case MTL::PixelFormatASTC_6x5_sRGB:
    case MTL::PixelFormatASTC_6x6_sRGB:
    case MTL::PixelFormatASTC_8x5_sRGB:
    case MTL::PixelFormatASTC_8x6_sRGB:
    case MTL::PixelFormatASTC_8x8_sRGB:
    case MTL::PixelFormatASTC_10x5_sRGB:
    case MTL::PixelFormatASTC_10x6_sRGB:
    case MTL::PixelFormatASTC_10x8_sRGB:
    case MTL::PixelFormatASTC_10x10_sRGB:
    case MTL::PixelFormatASTC_12x10_sRGB:
    case MTL::PixelFormatASTC_12x12_sRGB:
      return 0;
    case MTL::PixelFormatASTC_4x4_LDR:
    case MTL::PixelFormatASTC_5x4_LDR:
    case MTL::PixelFormatASTC_5x5_LDR:
    case MTL::PixelFormatASTC_6x5_LDR:
    case MTL::PixelFormatASTC_6x6_LDR:
    case MTL::PixelFormatASTC_8x5_LDR:
    case MTL::PixelFormatASTC_8x6_LDR:
    case MTL::PixelFormatASTC_8x8_LDR:
    case MTL::PixelFormatASTC_10x5_LDR:
    case MTL::PixelFormatASTC_10x6_LDR:
    case MTL::PixelFormatASTC_10x8_LDR:
    case MTL::PixelFormatASTC_10x10_LDR:
    case MTL::PixelFormatASTC_12x10_LDR:
    case MTL::PixelFormatASTC_12x12_LDR:
      return 0;
    // ASTC HDR (High Dynamic Range) Formats
    case MTL::PixelFormatASTC_4x4_HDR:
    case MTL::PixelFormatASTC_5x4_HDR:
    case MTL::PixelFormatASTC_5x5_HDR:
    case MTL::PixelFormatASTC_6x5_HDR:
    case MTL::PixelFormatASTC_6x6_HDR:
    case MTL::PixelFormatASTC_8x5_HDR:
    case MTL::PixelFormatASTC_8x6_HDR:
    case MTL::PixelFormatASTC_8x8_HDR:
    case MTL::PixelFormatASTC_10x5_HDR:
    case MTL::PixelFormatASTC_10x6_HDR:
    case MTL::PixelFormatASTC_10x8_HDR:
    case MTL::PixelFormatASTC_10x10_HDR:
    case MTL::PixelFormatASTC_12x10_HDR:
    case MTL::PixelFormatASTC_12x12_HDR:
    case MTL::PixelFormatGBGR422:
    case MTL::PixelFormatBGRG422:
      return 0;
    /* Depth */
    case MTL::PixelFormatDepth16Unorm:
      return 16;
    case MTL::PixelFormatDepth32Float:
      return 32;
    /* Stencil */
    case MTL::PixelFormatStencil8:
      return 8;
    /* Depth Stencil */
    case MTL::PixelFormatDepth24Unorm_Stencil8:
      return 32;
    case MTL::PixelFormatDepth32Float_Stencil8:
      return 40;
    case MTL::PixelFormatX32_Stencil8:
      return 40;
    case MTL::PixelFormatX24_Stencil8:
      return 32;
    }
  return 0;
  }

MTL::Texture& MtTexture::view(ComponentMapping m, uint32_t mipLevel) {
  if(m.r==ComponentSwizzle::Identity &&
     m.g==ComponentSwizzle::Identity &&
     m.b==ComponentSwizzle::Identity &&
     m.a==ComponentSwizzle::Identity &&
     (mipLevel==uint32_t(-1) || mipCnt==1)) {
    return *impl;
    }

  std::lock_guard<Detail::SpinLock> guard(syncViews);
  for(auto& i:extViews) {
    if(i.m==m && i.mip==mipLevel)
      return *i.v;
    }

  MTL::TextureSwizzleChannels sw;
  sw.red   = swizzle(m.r, MTL::TextureSwizzleRed);
  sw.green = swizzle(m.g, MTL::TextureSwizzleGreen);
  sw.blue  = swizzle(m.b, MTL::TextureSwizzleBlue);
  sw.alpha = swizzle(m.a, MTL::TextureSwizzleAlpha);

  auto levels = NS::Range(0, impl->mipmapLevelCount());
  if(mipLevel!=uint32_t(-1)) {
    levels = NS::Range(mipLevel,1);
    }

  View v;
  v.v   = NsPtr<MTL::Texture>(impl->newTextureView(impl->pixelFormat(),impl->textureType(),
                              levels,NS::Range(0,impl->arrayLength()),sw));
  v.m   = m;
  v.mip = mipLevel;

  if(v.v==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  try {
    extViews.push_back(std::move(v));
    }
  catch (...) {
    throw;
    }
  return *extViews.back().v;
  }

#endif
