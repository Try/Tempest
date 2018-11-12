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
  dev->destroy(*this);
  }

Uniforms& Uniforms::operator=(Uniforms &&u) {
  dev =u.dev;
  desc=std::move(u.desc);
  return *this;
  }

void Uniforms::set(size_t layoutBind, const Texture2d &tex) {
  desc.handler->set(layoutBind,tex.impl.handler);
  }

void Uniforms::set(size_t layoutBind, const Detail::ResourcePtr<Texture2d> &tex) {
  desc.handler->set(layoutBind,tex.impl.handler);
  }

void Uniforms::set(size_t layoutBind, const UniformBuffer &vbuf, size_t offset, size_t size) {
  desc.handler->set(layoutBind,vbuf.impl.impl.handler,offset,size);
  }
