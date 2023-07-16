#if defined(TEMPEST_BUILD_METAL)

#include "mtsamplercache.h"

#include "mtdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

namespace Tempest {
namespace Detail  {

static MTL::SamplerAddressMode nativeFormat(ClampMode m) {
  switch(m) {
    case ClampMode::ClampToBorder:  return MTL::SamplerAddressModeClampToBorderColor;
    case ClampMode::ClampToEdge:    return MTL::SamplerAddressModeClampToEdge;
    case ClampMode::MirroredRepeat: return MTL::SamplerAddressModeMirrorRepeat;
    case ClampMode::Repeat:         return MTL::SamplerAddressModeRepeat;
    case ClampMode::Count:          return MTL::SamplerAddressModeRepeat;
    }
  return MTL::SamplerAddressModeRepeat;
  }

static MTL::SamplerMinMagFilter nativeFormat(Tempest::Filter f) {
  switch(f) {
    case Filter::Nearest: return MTL::SamplerMinMagFilterNearest;
    case Filter::Linear:  return MTL::SamplerMinMagFilterLinear;
    case Filter::Count:   return MTL::SamplerMinMagFilterLinear;
    }
  return MTL::SamplerMinMagFilterLinear;
  }
}
}

MtSamplerCache::MtSamplerCache(MTL::Device& dev)
  :dev(dev) {
  def = mkSampler(Sampler(),false);
  if(def==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  }

MtSamplerCache::~MtSamplerCache() {
  }

MTL::SamplerState& MtSamplerCache::get(Tempest::Sampler src, bool argBuffers) {
  src.mapping = ComponentMapping(); // handled in imageview
  static const Tempest::Sampler defSrc;
  if(src==defSrc)
    return *def;

  std::lock_guard<std::mutex> guard(sync);
  for(auto& i:values)
    if(i.src==src && i.argBuffers==argBuffers)
      return *i.val;
  values.emplace_back(Entry());
  auto& b = values.back();
  b.src        = src;
  b.argBuffers = argBuffers;
  b.val        = mkSampler(src,argBuffers);
  if(b.val==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::OutOfHostMemory);
  return *b.val;
  }

NsPtr<MTL::SamplerState> MtSamplerCache::mkSampler(const Tempest::Sampler& src, bool argBuffers) {
  auto desc = NsPtr<MTL::SamplerDescriptor>::init();
  desc->setRAddressMode(nativeFormat(src.uClamp));
  desc->setSAddressMode(nativeFormat(src.vClamp));
  desc->setTAddressMode(nativeFormat(src.wClamp));

  desc->setMinFilter(nativeFormat(src.minFilter));
  desc->setMagFilter(nativeFormat(src.magFilter));
  if(src.mipFilter==Filter::Nearest)
    desc->setMipFilter(MTL::SamplerMipFilterNearest); else
    desc->setMipFilter(MTL::SamplerMipFilterLinear);
  desc->setMaxAnisotropy(src.anisotropic ? 16 : 1);
  desc->setBorderColor(MTL::SamplerBorderColorOpaqueWhite);
  desc->setLodAverage(false);
  desc->setSupportArgumentBuffers(argBuffers);

  return NsPtr<MTL::SamplerState>(dev.newSamplerState(desc.get()));
  }

#endif
