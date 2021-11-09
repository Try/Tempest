#include "abstractgraphicsapi.h"

using namespace Tempest;

static Sampler2d mkTrillinear() {
  Sampler2d s;
  s.anisotropic = false;
  return s;
  }

static Sampler2d mkNearest() {
  Sampler2d s;
  s.minFilter   = Filter::Nearest;
  s.magFilter   = Filter::Nearest;
  s.mipFilter   = Filter::Nearest;
  s.anisotropic = false;
  return s;
  }

static Sampler2d mkBilinear() {
  Sampler2d s;
  s.minFilter   = Filter::Linear;
  s.magFilter   = Filter::Linear;
  s.mipFilter   = Filter::Nearest;
  s.anisotropic = false;
  return s;
  }

const Sampler2d& Sampler2d::anisotrophy() {
  static Sampler2d s = Sampler2d();
  return s;
  }

const Sampler2d& Sampler2d::bilinear() {
  static Sampler2d s = mkBilinear();
  return s;
  }

const Sampler2d& Sampler2d::trillinear() {
  static Sampler2d s = mkTrillinear();
  return s;
  }

const Sampler2d& Sampler2d::nearest() {
  static Sampler2d s = mkNearest();
  return s;
  }

bool AbstractGraphicsApi::Props::hasSamplerFormat(TextureFormat f) const {
  uint64_t  m = uint64_t(1) << uint64_t(f);
  return (smpFormat&m)!=0;
  }

bool AbstractGraphicsApi::Props::hasAttachFormat(TextureFormat f) const {
  uint64_t  m = uint64_t(1) << uint64_t(f);
  return (attFormat&m)!=0;
  }

bool AbstractGraphicsApi::Props::hasDepthFormat(TextureFormat f) const {
  uint64_t  m = uint64_t(1) << uint64_t(f);
  return (dattFormat&m)!=0;
  }

bool AbstractGraphicsApi::Props::hasStorageFormat(TextureFormat f) const {
  uint64_t  m = uint64_t(1) << uint64_t(f);
  return (storFormat&m)!=0;
  }

void AbstractGraphicsApi::CommandBuffer::barrier(Texture& tex, ResourceAccess prev, ResourceAccess next, uint32_t mipId) {
  AbstractGraphicsApi::BarrierDesc b;
  b.texture  = &tex;
  b.prev     = prev;
  b.next     = next;
  b.mip      = mipId;
  b.discard  = (prev==ResourceAccess::None);
  barrier(&b,1);
  }
