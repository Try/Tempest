#pragma once

#include "videobuffer.h"

namespace Tempest {

template<class T>
class UniformBuffer final {
  public:
    UniformBuffer()=default;
    UniformBuffer(UniformBuffer&&)=default;
    UniformBuffer& operator=(UniformBuffer&&)=default;

    bool isEmpty() const { return impl.size()==0; }
    void update(const T* data) { return impl.update(data,0,1,sizeof(T),sizeof(T)); }

  private:
    UniformBuffer(Tempest::Detail::VideoBuffer&& impl)
      :impl(std::move(impl)) {
      }

    Tempest::Detail::VideoBuffer impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::DescriptorSet;
  };

}
