#include "mtdescriptorarray.h"

#include "mttexture.h"
#include "mtpipelinelay.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtDescriptorArray::MtDescriptorArray(const MtPipelineLay &lay)
  :lay(&lay) {
  desc.reset(new Desc[lay.lay.size()]);
  }

void MtDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture *tex, const Sampler2d &smp) {

  }

void MtDescriptorArray::setSsbo(size_t id, AbstractGraphicsApi::Texture *tex, uint32_t mipLevel) {
  auto& t = *reinterpret_cast<MtTexture*>(tex);
  desc[id].val = t.impl;
  }

void MtDescriptorArray::setUbo(size_t id, AbstractGraphicsApi::Buffer *buf, size_t offset) {

  }

void MtDescriptorArray::setSsbo(size_t id, AbstractGraphicsApi::Buffer *buf, size_t offset) {

  }

void MtDescriptorArray::ssboBarriers(ResourceState &res) {

  }
