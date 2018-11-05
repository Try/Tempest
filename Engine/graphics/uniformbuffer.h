#pragma once

#include "videobuffer.h"

namespace Tempest {

class UniformBuffer {
  public:
    UniformBuffer()=default;
    UniformBuffer(UniformBuffer&&)=default;
    UniformBuffer& operator=(UniformBuffer&&)=default;

  private:
    UniformBuffer(Tempest::VideoBuffer&& impl)
      :impl(std::move(impl)) {
      }

    Tempest::VideoBuffer impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Uniforms;
  };

}
