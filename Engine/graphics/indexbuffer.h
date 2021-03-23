#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "videobuffer.h"

namespace Tempest {

class Device;

namespace Detail {
  template<class T>
  struct IsIndexType:std::false_type{};

  template<>
  struct IsIndexType<uint16_t>:std::true_type{};

  template<>
  struct IsIndexType<uint32_t>:std::true_type{};
  }

template<class T>
class IndexBuffer final {
  public:
    static_assert(Detail::IsIndexType<T>::value,"unsupported index type");

    IndexBuffer()=default;
    IndexBuffer(IndexBuffer&&)=default;
    IndexBuffer& operator=(IndexBuffer&&)=default;

    size_t size() const { return sz; }
    void   update(const std::vector<T>& v)                 { return this->impl.update(v.data(),0,v.size(),sizeof(T),sizeof(T)); }
    void   update(const T* data,size_t offset,size_t size) { return this->impl.update(data,offset,size,sizeof(T),sizeof(T)); }

  private:
    IndexBuffer(Tempest::VideoBuffer&& impl,size_t size)
      :impl(std::move(impl)),sz(size) {
      }

    Tempest::VideoBuffer impl;
    size_t               sz=0;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

}
