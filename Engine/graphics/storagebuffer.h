#pragma once

#include "videobuffer.h"

namespace Tempest {

class StorageBuffer {
  public:
    StorageBuffer()=default;
    StorageBuffer(StorageBuffer&&)=default;
    StorageBuffer& operator=(StorageBuffer&&)=default;
    virtual ~StorageBuffer() = default;

    bool   isEmpty()  const { return impl.size()==0; }
    size_t byteSize() const { return impl.size();    }

    template<class T>
    void   update(const std::vector<T>& v)                      { return impl.update(v.data(),0,v.size()*sizeof(T),1,1); }
    void   update(const void* data, size_t offset, size_t size) { return impl.update(data,offset,size,1,1); }

  private:
    explicit StorageBuffer(Tempest::Detail::VideoBuffer&& impl)
      :impl(std::move(impl)) {
      }

    Tempest::Detail::VideoBuffer impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::DescriptorSet;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend class VertexBuffer;

  template<class T>
  friend class IndexBuffer;
  };
}
