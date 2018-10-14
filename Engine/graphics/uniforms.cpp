#include "uniforms.h"

using namespace Tempest;

Uniforms::Uniforms(Tempest::VideoBuffer &&impl)
  :impl(std::move(impl)) {
  }
