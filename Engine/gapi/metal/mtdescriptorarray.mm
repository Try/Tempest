#include "mtdescriptorarray.h"

#include "mtdevice.h"
#include "mttexture.h"
#include "mtbuffer.h"
#include "mtpipelinelay.h"

#import <Metal/MTLSampler.h>
#import <Metal/Metal.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtDescriptorArray::MtDescriptorArray(MtDevice& dev, const MtPipelineLay &lay)
  :dev(dev), lay(&lay) {
  desc.reset(new Desc[lay.lay.size()]);
  }

void MtDescriptorArray::set(size_t i, AbstractGraphicsApi::Texture *tex, const Sampler2d &smp) {
  auto& t = *reinterpret_cast<MtTexture*>(tex);
  desc[i].val     = t.impl;
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
