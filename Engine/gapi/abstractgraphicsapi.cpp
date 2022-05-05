#include "abstractgraphicsapi.h"

#include <Tempest/Except>

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

void AbstractGraphicsApi::CommandBuffer::dispatchMesh(size_t firstInstance, size_t instanceCount) {
  throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension);
  }

AbstractGraphicsApi::AccelerationStructure*
  AbstractGraphicsApi::createBottomAccelerationStruct(Device* d,
                                                      Buffer* vbo, size_t vboSz, size_t stride,
                                                      Buffer* ibo, size_t iboSz, size_t offset, Detail::IndexClass icls) {
  throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension);
  }

AbstractGraphicsApi::AccelerationStructure*
  AbstractGraphicsApi::createTopAccelerationStruct(Device* d, const RtInstance* geom, AccelerationStructure*const* as, size_t geomSize) {
  throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension);
  }
