#include "mtsamplercache.h"

#include "mtdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

namespace Tempest {
namespace Detail  {

static MTLSamplerAddressMode nativeFormat(ClampMode m) {
  switch(m) {
    case ClampMode::ClampToBorder:  return MTLSamplerAddressModeClampToBorderColor;
    case ClampMode::ClampToEdge:    return MTLSamplerAddressModeClampToEdge;
    case ClampMode::MirroredRepeat: return MTLSamplerAddressModeMirrorRepeat;
    case ClampMode::Repeat:         return MTLSamplerAddressModeRepeat;
    case ClampMode::Count:          return MTLSamplerAddressModeRepeat;
    }
  return MTLSamplerAddressModeRepeat;
  }

static MTLSamplerMinMagFilter nativeFormat(Tempest::Filter f) {
  switch(f) {
    case Filter::Nearest: return MTLSamplerMinMagFilterNearest;
    case Filter::Linear:  return MTLSamplerMinMagFilterLinear;
    case Filter::Count:   return MTLSamplerMinMagFilterLinear;
    }
  return MTLSamplerMinMagFilterLinear;
  }
}
}

MtSamplerCache::MtSamplerCache(id<MTLDevice> dev)
  :dev(dev) {
  def = mkSampler(Sampler2d());
  if(def==nil)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  }

MtSamplerCache::~MtSamplerCache() {
  [def release];
  for(auto& i:values)
    [i.val release];
  }

id<MTLSamplerState> MtSamplerCache::get(Tempest::Sampler2d src) {
  src.mapping = ComponentMapping(); // handled in imageview
  static const Tempest::Sampler2d defSrc;
  if(src==defSrc)
    return def;

  std::lock_guard<SpinLock> guard(sync);
  for(auto& i:values)
    if(i.src==src)
      return i.val;
  values.emplace_back(Entry());
  auto& b = values.back();
  b.src = src;
  b.val = mkSampler(src);
  if(b.val==nil)
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
  return b.val;
  }

id<MTLSamplerState> MtSamplerCache::mkSampler(const Tempest::Sampler2d& src) {
  MTLSamplerDescriptor* sdesc = [MTLSamplerDescriptor new];
  sdesc.rAddressMode  = nativeFormat(src.uClamp);
  sdesc.sAddressMode  = nativeFormat(src.vClamp);
  sdesc.tAddressMode  = MTLSamplerAddressModeRepeat;
  sdesc.minFilter     = nativeFormat(src.minFilter);
  sdesc.magFilter     = nativeFormat(src.magFilter);
  sdesc.mipFilter     = MTLSamplerMipFilterNotMipmapped;
  if(src.mipFilter==Filter::Nearest)
    sdesc.mipFilter = MTLSamplerMipFilterNearest; else
    sdesc.mipFilter = MTLSamplerMipFilterLinear;
  sdesc.maxAnisotropy = src.anisotropic ? 16 : 1;
  sdesc.borderColor   = MTLSamplerBorderColorOpaqueWhite;
  sdesc.lodAverage    = NO;

  sdesc.supportArgumentBuffers = NO;

  id<MTLSamplerState> sampler = [dev newSamplerStateWithDescriptor:sdesc];
  [sdesc release];
  return sampler;
  }
