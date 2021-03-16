#pragma once

#include "videobuffer.h"

namespace Tempest {

class StorageBuffer final {
  public:
    StorageBuffer()=default;
    StorageBuffer(StorageBuffer&&)=default;
    StorageBuffer& operator=(StorageBuffer&&)=default;

    // void   update(const T* data,size_t offset,size_t size) { return impl.update(data,offset,size,sizeof(T),alignedTSZ); }
    size_t size() const { return impl.size(); }

  private:
    StorageBuffer(Tempest::VideoBuffer&& impl)
      :impl(std::move(impl)) {
      }

    Tempest::VideoBuffer impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Uniforms;
  };

}
