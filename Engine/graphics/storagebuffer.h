#pragma once

#include "videobuffer.h"

namespace Tempest {

template<class T>
class StorageBuffer final {
  public:
    StorageBuffer()=default;
    StorageBuffer(StorageBuffer&&)=default;
    StorageBuffer& operator=(StorageBuffer&&)=default;

    // void   update(const T* data,size_t offset,size_t size) { return impl.update(data,offset,size,sizeof(T),alignedTSZ); }
    size_t size() const { return arrTSZ; }

  private:
    StorageBuffer(Tempest::VideoBuffer&& impl, size_t alignedTSZ)
      :impl(std::move(impl)), alignedTSZ(alignedTSZ), arrTSZ(impl.size()/alignedTSZ) {
      }

    Tempest::VideoBuffer impl;
    size_t               alignedTSZ=0;
    size_t               arrTSZ=0;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Uniforms;
  };

}
