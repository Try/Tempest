#include "abstractgraphicsapi.h"

#include <Tempest/Except>

using namespace Tempest;

static Sampler mkTrillinear() {
  Sampler s;
  s.anisotropic = false;
  return s;
  }

static Sampler mkNearest() {
  Sampler s;
  s.minFilter   = Filter::Nearest;
  s.magFilter   = Filter::Nearest;
  s.mipFilter   = Filter::Nearest;
  s.anisotropic = false;
  return s;
  }

static Sampler mkBilinear() {
  Sampler s;
  s.minFilter   = Filter::Linear;
  s.magFilter   = Filter::Linear;
  s.mipFilter   = Filter::Nearest;
  s.anisotropic = false;
  return s;
  }

const Sampler& Sampler::anisotrophy() {
  static Sampler s = Sampler();
  return s;
  }

const Sampler& Sampler::bilinear() {
  static Sampler s = mkBilinear();
  return s;
  }

const Sampler& Sampler::trillinear() {
  static Sampler s = mkTrillinear();
  return s;
  }

const Sampler& Sampler::nearest() {
  static Sampler s = mkNearest();
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

void AbstractGraphicsApi::CommandBuffer::setDebugMarker(std::string_view tag) {
  (void)tag;
  }

void AbstractGraphicsApi::CommandBuffer::dispatchMesh(size_t x, size_t y, size_t z) {
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

void AbstractGraphicsApi::Desc::set(size_t id, Texture** tex, size_t cnt, const Sampler& smp, uint32_t mipLevel) {
  throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension);
  }

void Tempest::AbstractGraphicsApi::Desc::set(size_t id, Buffer** buf, size_t cnt) {
  throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension);
  }

void AbstractGraphicsApi::Desc::ssboBarriers(Detail::ResourceState&, PipelineStage) {
  // NOP by default
  }

void AbstractGraphicsApi::Buffer::update(const void* data, size_t off, size_t size) {
  this->update(data,off,size,1,1);
  }
