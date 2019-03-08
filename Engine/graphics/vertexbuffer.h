#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "videobuffer.h"

namespace Tempest {

class Device;

template<class T>
class VertexBuffer final {
  public:
    VertexBuffer()=default;
    VertexBuffer(VertexBuffer&&)=default;
    VertexBuffer& operator=(VertexBuffer&&)=default;

    size_t size() const { return sz; }

  private:
    VertexBuffer(Tempest::VideoBuffer&& impl,size_t size)
      :impl(std::move(impl)),sz(size) {
      }

    Tempest::VideoBuffer impl;
    size_t               sz=0;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  };

template<class T>
inline std::initializer_list<Decl::ComponentType> vertexBufferDecl(){ return{}; };

}
