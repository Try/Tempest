#include "uniforms.h"

using namespace Tempest;

Uniforms::Uniforms(Tempest::VideoBuffer &&impl, Device &dev, AbstractGraphicsApi::Desc *desc)
  :dev(&dev),desc(desc),impl(std::move(impl)) {
  }

Uniforms::Uniforms(Uniforms && u)
  :dev(u.dev), desc(std::move(u.desc)), impl(std::move(u.impl)) {

  }

Uniforms::~Uniforms() {
  dev->destroy(*this);
  }

Uniforms& Uniforms::operator=(Uniforms &&u) {
  dev =u.dev;
  desc=std::move(u.desc);
  impl=std::move(u.impl);
  return *this;
  }
