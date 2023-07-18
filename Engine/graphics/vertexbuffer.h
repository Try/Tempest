#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/StorageBuffer>
#include <vector>

namespace Tempest {

class Device;

template<class T>
class Encoder;

template<class T>
class VertexBuffer : public StorageBuffer {
  public:
    VertexBuffer()=default;
    VertexBuffer(VertexBuffer&&)=default;
    VertexBuffer& operator=(VertexBuffer&&)=default;

    size_t size() const { return sz; }

  private:
    VertexBuffer(Tempest::Detail::VideoBuffer&& impl,size_t size)
      :StorageBuffer(std::move(impl)),sz(size) {
      }

    size_t sz = 0;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  friend class Tempest::DescriptorSet;
  };

}
