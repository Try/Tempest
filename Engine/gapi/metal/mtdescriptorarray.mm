#include "mtdescriptorarray.h"

#include "mtdevice.h"
#include "mttexture.h"
#include "mtbuffer.h"
#include "mtpipelinelay.h"
#include "mtaccelerationstructure.h"

#import <Metal/MTLTexture.h>
#import <Metal/MTLSampler.h>
#import <Metal/Metal.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtDescriptorArray::MtDescriptorArray(MtDevice& dev, const MtPipelineLay &lay)
  :dev(dev), lay(&lay) {
  desc.reset(new Desc[lay.lay.size()]);
  }

void MtDescriptorArray::set(size_t i, AbstractGraphicsApi::Texture *tex, const Sampler2d &smp, uint32_t mipLevel) {
  auto& t = *reinterpret_cast<MtTexture*>(tex);
  desc[i].val     = t.view(smp.mapping,mipLevel);
  desc[i].sampler = dev.samplers.get(smp);
  }

void MtDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer *buf, size_t offset) {
  auto& b = *reinterpret_cast<MtBuffer*>(buf);
  desc[id].val    = b.impl;
  desc[id].offset = offset;
  }

void MtDescriptorArray::setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure* a) {
  auto& as = *reinterpret_cast<MtAccelerationStructure*>(a);
  desc[id].val = as.impl;
  }

void MtDescriptorArray::ssboBarriers(ResourceState&) {
  }
