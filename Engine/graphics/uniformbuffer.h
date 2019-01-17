#pragma once

#include "videobuffer.h"

namespace Tempest {

class UniformBuffer {
  public:
    UniformBuffer()=default;
    UniformBuffer(UniformBuffer&&)=default;
    UniformBuffer& operator=(UniformBuffer&&)=default;

    void   update(const void* data,size_t offset,size_t size) { return impl.update(data,offset,size); }
    size_t size() const { return impl.size(); }

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
