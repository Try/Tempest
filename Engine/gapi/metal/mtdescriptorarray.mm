#include "mtdescriptorarray.h"

#include "mtdevice.h"
#include "mttexture.h"
#include "mtbuffer.h"
#include "mtpipelinelay.h"

#import <Metal/MTLTexture.h>
#import <Metal/MTLSampler.h>
#import <Metal/Metal.h>

using namespace Tempest;
using namespace Tempest::Detail;

static MTLTextureSwizzle swizzle(ComponentSwizzle cs, MTLTextureSwizzle def){
  switch(cs) {
    case ComponentSwizzle::Identity:
      return def;
    case ComponentSwizzle::R:
      return MTLTextureSwizzleRed;
    case ComponentSwizzle::G:
      return MTLTextureSwizzleGreen;
    case ComponentSwizzle::B:
      return MTLTextureSwizzleBlue;
    case ComponentSwizzle::A:
      return MTLTextureSwizzleAlpha;
    }
  return def;
  }

MtDescriptorArray::MtDescriptorArray(MtDevice& dev, const MtPipelineLay &lay)
  :dev(dev), lay(&lay) {
  desc.reset(new Desc[lay.lay.size()]);
  }

void MtDescriptorArray::set(size_t i, AbstractGraphicsApi::Texture *tex, const Sampler2d &smp) {
  auto& t = *reinterpret_cast<MtTexture*>(tex);
  if(smp.mapping==ComponentMapping()) {
    desc[i].val = t.impl;
    } else {
    auto sw = MTLTextureSwizzleChannelsMake(swizzle(smp.mapping.r,MTLTextureSwizzleRed),
                                            swizzle(smp.mapping.r,MTLTextureSwizzleGreen),
                                            swizzle(smp.mapping.r,MTLTextureSwizzleBlue),
                                            swizzle(smp.mapping.r,MTLTextureSwizzleAlpha));

    id<MTLTexture> view = [t.impl newTextureViewWithPixelFormat: t.impl.pixelFormat
                                  textureType: t.impl.textureType
                                  levels: NSMakeRange(0, t.impl.mipmapLevelCount)
                                  slices: NSMakeRange(0, t.impl.arrayLength)
                                  swizzle:sw];
    if(view==nil)
      throw std::system_error(GraphicsErrc::OutOfHostMemory);

    desc[i].val = view;
    }
  desc[i].sampler = dev.samplers.get(smp);
  }

void MtDescriptorArray::setSsbo(size_t id, AbstractGraphicsApi::Texture *tex, uint32_t mipLevel) {
  auto& t = *reinterpret_cast<MtTexture*>(tex);
  desc[id].val      = t.impl;
  desc[id].mipLevel = mipLevel;
  }

void MtDescriptorArray::setUbo(size_t id, AbstractGraphicsApi::Buffer *buf, size_t offset) {
  auto& b = *reinterpret_cast<MtBuffer*>(buf);
  desc[id].val    = b.impl;
  desc[id].offset = offset;
  }

void MtDescriptorArray::setSsbo(size_t id, AbstractGraphicsApi::Buffer *buf, size_t offset) {
  auto& b = *reinterpret_cast<MtBuffer*>(buf);
  desc[id].val    = b.impl;
  desc[id].offset = offset;
  }

void MtDescriptorArray::ssboBarriers(ResourceState&) {
  }
