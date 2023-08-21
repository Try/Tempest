#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/IndexBuffer>

namespace Tempest {

template<class T>
class VertexBuffer;

template<class T>
class IndexBuffer;

class StorageBuffer;

class AccelerationStructure;

enum class RtInstanceFlags : uint8_t {
  Opaque    = 0,
  NonOpaque = 1,
  };

inline RtInstanceFlags operator | (RtInstanceFlags a, RtInstanceFlags b){
  return RtInstanceFlags(uint16_t(a)|uint16_t(b));
  }

inline RtInstanceFlags operator & (RtInstanceFlags a, RtInstanceFlags b){
  return RtInstanceFlags(uint16_t(a)&uint16_t(b));
  }

class RtInstance {
  public:
  Tempest::Matrix4x4           mat   = Matrix4x4::mkIdentity();
  uint32_t                     id    = 0;
  RtInstanceFlags              flags = RtInstanceFlags::Opaque;
  const AccelerationStructure* blas  = nullptr;
  };

class RtGeometry {
  public:
  RtGeometry() = default;

  template<class V, class I>
  RtGeometry(const VertexBuffer<V>& vbo, const IndexBuffer<I>& ibo):
      vbo(&vbo),vboStride(sizeof(V)),ibo(&ibo),icls(Detail::indexCls<I>()),iboOffset(0),iboSize(ibo.size()){}

  template<class V, class I>
  RtGeometry(const VertexBuffer<V>& vbo, const IndexBuffer<I>& ibo, size_t offset, size_t count):
      vbo(&vbo),vboStride(sizeof(V)),ibo(&ibo),icls(Detail::indexCls<I>()),iboOffset(offset),iboSize(count){}

  const StorageBuffer* vbo       = nullptr;
  size_t               vboStride = 0;
  const StorageBuffer* ibo       = nullptr;
  Detail::IndexClass   icls      = Detail::IndexClass::i32;
  size_t               iboOffset = 0;
  size_t               iboSize   = 0;
  };

class AccelerationStructure final {
  public:
    AccelerationStructure() = default;
    AccelerationStructure(AccelerationStructure&&)=default;
    ~AccelerationStructure()=default;
    AccelerationStructure& operator=(AccelerationStructure&&)=default;

    bool isEmpty() const;

  private:
    AccelerationStructure(Tempest::Device& dev, AbstractGraphicsApi::AccelerationStructure* impl);

    Detail::DSharedPtr<AbstractGraphicsApi::AccelerationStructure*> impl;

  friend class Tempest::Device;
  friend class Tempest::DescriptorSet;
  friend class Encoder<Tempest::CommandBuffer>;
  };

}

