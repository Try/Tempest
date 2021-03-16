#include "uniforms.h"

#include <Tempest/Device>
#include <Tempest/UniformBuffer>

using namespace Tempest;

Uniforms::Uniforms(Device& dev, AbstractGraphicsApi::Desc *desc)
  :dev(&dev),desc(desc) {
  }

Uniforms::Uniforms(Uniforms && u)
  :dev(u.dev), desc(std::move(u.desc)) {
  }

Uniforms::~Uniforms() {
  delete desc.handler;
  }

Uniforms& Uniforms::operator=(Uniforms &&u) {
  dev =u.dev;
  desc=std::move(u.desc);
  return *this;
  }

void Uniforms::set(size_t layoutBind, const Texture2d &tex, const Sampler2d& smp) {
  if(tex.impl.handler)
    desc.handler->set(layoutBind,tex.impl.handler,smp); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void Uniforms::set(size_t layoutBind, const Attachment& tex, const Sampler2d& smp) {
  if(tex.tImpl.impl.handler)
    desc.handler->set(layoutBind,tex.tImpl.impl.handler,smp); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void Uniforms::set(size_t layoutBind, const StorageImage& tex, uint32_t mipLevel) {
  if(tex.impl.handler)
    desc.handler->setSsbo(layoutBind,tex.impl.handler,mipLevel); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void Uniforms::set(size_t layoutBind, const Detail::ResourcePtr<Texture2d> &tex, const Sampler2d& smp) {
  if(tex.impl.handler)
    desc.handler->set(layoutBind,tex.impl.handler,smp); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void Uniforms::set(size_t layoutBind, const StorageBuffer& vbuf) {
  implBindSsbo(layoutBind,vbuf.impl,0);
  }

void Uniforms::set(size_t layoutBind, const StorageBuffer& vbuf, size_t offset) {
  implBindSsbo(layoutBind,vbuf.impl,offset);
  }

void Uniforms::implBindUbo(size_t layoutBind, const VideoBuffer& vbuf, size_t offsetBytes) {
  if(vbuf.impl.handler)
    desc.handler->setUbo(layoutBind,vbuf.impl.handler,offsetBytes); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer);
  }

void Uniforms::implBindSsbo(size_t layoutBind, const VideoBuffer& vbuf, size_t offsetBytes) {
  if(vbuf.impl.handler)
    desc.handler->setSsbo(layoutBind,vbuf.impl.handler,offsetBytes); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer);
  }
