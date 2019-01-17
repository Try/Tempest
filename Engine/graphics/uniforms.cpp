#include "uniforms.h"

#include <Tempest/UniformBuffer>

using namespace Tempest;

Uniforms::Uniforms(Device &dev, AbstractGraphicsApi::Desc *desc)
  :dev(&dev),desc(desc) {
  }

Uniforms::Uniforms(Uniforms && u)
  :dev(u.dev), desc(std::move(u.desc)) {
  }

Uniforms::~Uniforms() {
  if(dev)
    dev->destroy(*this);
  }

Uniforms& Uniforms::operator=(Uniforms &&u) {
  dev =u.dev;
  desc=std::move(u.desc);
  return *this;
  }

void Uniforms::set(size_t layoutBind, const UniformBuffer &vbuf) {
  set(layoutBind,vbuf,0,vbuf.size());
  }

void Uniforms::set(size_t layoutBind, const Texture2d &tex) {
  if(tex.impl.handler)
    desc.handler->set(layoutBind,tex.impl.handler);
  }

void Uniforms::set(size_t layoutBind, const Detail::ResourcePtr<Texture2d> &tex) {
  if(tex.impl.handler)
    desc.handler->set(layoutBind,tex.impl.handler);
  }

void Uniforms::set(size_t layoutBind, const UniformBuffer &vbuf, size_t offset, size_t size) {
  if(vbuf.impl.impl.handler)
    desc.handler->set(layoutBind,vbuf.impl.impl.handler,offset,size); else
    throw std::logic_error("invalid uniform buffer");
  }
