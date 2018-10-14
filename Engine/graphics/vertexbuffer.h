#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Device>
#include "videobuffer.h"

namespace Tempest {

class Device;

template<class T>
class VertexBuffer {
  public:
    VertexBuffer()=default;
    VertexBuffer(VertexBuffer&&)=default;
    VertexBuffer& operator=(VertexBuffer&&)=default;

  private:
    VertexBuffer(Tempest::VideoBuffer&& impl)
      :impl(std::move(impl)) {
      }

    Tempest::VideoBuffer impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  };

}
