#pragma once

#include "videobuffer.h"

namespace Tempest {

class StorageBuffer final {
  public:
    StorageBuffer()=default;
    StorageBuffer(StorageBuffer&&)=default;
    StorageBuffer& operator=(StorageBuffer&&)=default;

    size_t size() const { return impl.size(); }

    template<class T>
    void   update(const std::vector<T>& v)                      { return impl.update(v.data(),0,v.size()*sizeof(T),1,1); }
    void   update(const void* data, size_t offset, size_t size) { return impl.update(data,offset,size,1,1); }

  private:
    StorageBuffer(Tempest::VideoBuffer&& impl)
      :impl(std::move(impl)) {
      }

    Tempest::VideoBuffer impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::DescriptorSet;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

}
