#include "uniforms.h"

#include <Tempest/Device>
#include <Tempest/UniformBuffer>
#include <Tempest/Except>

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

void Uniforms::set(size_t layoutBind, const Texture2d &tex) {
  if(tex.impl.handler)
    desc.handler->set(layoutBind,tex.impl.handler); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void Uniforms::set(size_t layoutBind, const Attachment& tex) {
  if(tex.tImpl.impl.handler)
    desc.handler->set(layoutBind,tex.tImpl.impl.handler); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void Uniforms::set(size_t layoutBind, const Detail::ResourcePtr<Texture2d> &tex) {
  if(tex.impl.handler)
    desc.handler->set(layoutBind,tex.impl.handler); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void Uniforms::implBindUbo(size_t layoutBind, const VideoBuffer& vbuf, size_t offset, size_t count, size_t size) {
  if(vbuf.impl.handler)
    desc.handler->set(layoutBind,vbuf.impl.handler,offset*size,count*size,size); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer);
  }
