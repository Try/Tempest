#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "videobuffer.h"
#include <vector>

namespace Tempest {

class Device;

template<class T>
class Encoder;

template<class T>
class VertexBuffer final {
  public:
    VertexBuffer()=default;
    VertexBuffer(VertexBuffer&&)=default;
    VertexBuffer& operator=(VertexBuffer&&)=default;

    size_t size() const { return sz; }

    void   update(const std::vector<T>& v)                 { return this->impl.update(v.data(),0,v.size(),sizeof(T),sizeof(T)); }
    void   update(const T* data,size_t offset,size_t size) { return this->impl.update(data,offset,size,sizeof(T),sizeof(T)); }

  private:
    VertexBuffer(Tempest::VideoBuffer&& impl,size_t size)
      :impl(std::move(impl)),sz(size) {
      }

    Tempest::VideoBuffer impl;
    size_t               sz=0;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  friend class Tempest::DescriptorSet;

  template<class T2>
  friend       VideoBuffer& bufferCast(VertexBuffer<T2>& s);

  template<class T2>
  friend const VideoBuffer& bufferCast(const VertexBuffer<T2>& s);
  };

template<class T>
inline VideoBuffer& bufferCast(VertexBuffer<T>& s) {
  return s.impl;
  }

template<class T>
inline const VideoBuffer& bufferCast(const VertexBuffer<T>& s) {
  return s.impl;
  }

}
