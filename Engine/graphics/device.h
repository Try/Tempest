#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/CommandBuffer>
#include <Tempest/RenderPipeline>
#include <Tempest/ComputePipeline>
#include <Tempest/Shader>
#include <Tempest/Attachment>
#include <Tempest/ZBuffer>
#include <Tempest/Texture2d>
#include <Tempest/DescriptorSet>
#include <Tempest/DescriptorArray>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/UniformBuffer>
#include <Tempest/StorageBuffer>
#include <Tempest/StorageImage>
#include <Tempest/AccelerationStructure>
#include <Tempest/Builtin>
#include <Tempest/Swapchain>
#include <Tempest/Except>

#include "videobuffer.h"

#include <vector>

namespace Tempest {

class Fence;

class CommandPool;
class RFile;

class Pixmap;

class DescriptorSet;
class PipelineLayout;

class Color;
class RenderState;

class Device {
  public:
    using Props=AbstractGraphicsApi::Props;

    Device(AbstractGraphicsApi& api);
    Device(AbstractGraphicsApi& api, std::string_view name);
    Device(AbstractGraphicsApi& api, DeviceType type);
    Device(const Device&)=delete;
    virtual ~Device();

    void                  waitIdle();

    void                  submit(const CommandBuffer& cmd);
    void                  submit(const CommandBuffer& cmd, Fence& fdone);
    void                  present(Swapchain& sw);

    Swapchain             swapchain(SystemApi::Window* w) const;

    Shader                shader(RFile&          file);
    Shader                shader(const char*     filename);
    Shader                shader(const char16_t* filename);
    Shader                shader(const void* source, const size_t length);

    const Props&          properties() const;

    template<class T>
    VertexBuffer<T>       vbo(const T* arr, size_t arrSize) {
      return vbo(BufferHeap::Device,arr,arrSize);
      }
    template<class T>
    VertexBuffer<T>       vbo(const std::vector<T>& arr){
      return vbo(BufferHeap::Device,arr.data(),arr.size());
      }
    template<class T>
    VertexBuffer<T>       vbo(BufferHeap ht, const T* arr,size_t arrSize);
    template<class T>
    VertexBuffer<T>       vbo(BufferHeap ht, const std::vector<T>& arr) {
      return vbo(ht,arr.data(),arr.size());
      }

    template<class T>
    IndexBuffer<T>        ibo(const T* arr, size_t arrSize) {
      return ibo(BufferHeap::Device,arr,arrSize);
      }
    template<class T>
    IndexBuffer<T>        ibo(const std::vector<T>& arr) {
      return ibo(BufferHeap::Device,arr.data(),arr.size());
      }
    template<class T>
    IndexBuffer<T>        ibo(BufferHeap ht, const T* arr, size_t arrSize);
    template<class T>
    IndexBuffer<T>        ibo(BufferHeap ht, const std::vector<T>& arr) {
      return ibo(ht,arr.data(),arr.size());
      }

    template<class T>
    UniformBuffer<T>      ubo(const T& data)  {
      return implUbo<T>(BufferHeap::Upload,&data);
      }
    template<class T>
    UniformBuffer<T>      ubo(BufferHeap ht, const T& data)  {
      return implUbo<T>(ht,&data);
      }

    StorageBuffer         ssbo(BufferHeap ht, const void* data, size_t size);
    template<class T>
    StorageBuffer         ssbo(BufferHeap ht, const std::vector<T>& arr) {
      return ssbo(ht,arr.data(),arr.size()*sizeof(T));
      }
    StorageBuffer         ssbo(BufferHeap ht, Uninitialized_t data, size_t size);
    StorageBuffer         ssbo(const void* data, size_t size) {
      return ssbo(BufferHeap::Device,data,size);
      }
    StorageBuffer         ssbo(Uninitialized_t data, size_t size) {
      return ssbo(BufferHeap::Device,data,size);
      }
    template<class T>
    StorageBuffer         ssbo(const std::vector<T>& arr) {
      return ssbo(BufferHeap::Device,arr.data(),arr.size()*sizeof(T));
      }

    DescriptorArray       descriptors(const std::vector<const StorageBuffer*>& buf);
    DescriptorArray       descriptors(const StorageBuffer* const *buf, size_t size);
    DescriptorArray       descriptors(const std::vector<const Texture2d*>& tex);
    DescriptorArray       descriptors(const Texture2d* const *tex, size_t size);
    DescriptorArray       descriptors(const std::vector<const Texture2d*>& tex, const Sampler& smp);
    DescriptorArray       descriptors(const Texture2d* const *tex, size_t size, const Sampler& smp);

    Texture2d             texture    (const Pixmap& pm, const bool mips = true);
    Attachment            attachment (TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips = false);
    ZBuffer               zbuffer    (TextureFormat frm, const uint32_t w, const uint32_t h);
    StorageImage          image2d    (TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips = false);
    StorageImage          image3d    (TextureFormat frm, const uint32_t w, const uint32_t h, const uint32_t d, const bool mips = false);

    AccelerationStructure blas(const std::vector<RtGeometry>& geom);
    AccelerationStructure blas(std::initializer_list<RtGeometry> geom);
    AccelerationStructure blas(const RtGeometry* geom, size_t geomSize);

    template<class V, class I>
    AccelerationStructure blas(const VertexBuffer<V>& vbo, const IndexBuffer<I>& ibo);
    template<class V, class I>
    AccelerationStructure blas(const VertexBuffer<V>& vbo, const IndexBuffer<I>& ibo, size_t offset, size_t count);

    AccelerationStructure tlas(std::initializer_list<RtInstance> geom);
    AccelerationStructure tlas(const std::vector<RtInstance>& geom);
    AccelerationStructure tlas(const RtInstance* geom, size_t geomSize);

    Pixmap                readPixels(const Texture2d&    t, uint32_t mip=0);
    Pixmap                readPixels(const Attachment&   t, uint32_t mip=0);
    Pixmap                readPixels(const StorageImage& t, uint32_t mip=0);
    void                  readBytes (const StorageBuffer& ssbo, void* out, size_t size);

    RenderPipeline        pipeline(Topology tp,const RenderState& st, const Shader &vs, const Shader &fs);
    RenderPipeline        pipeline(Topology tp,const RenderState& st, const Shader &vs, const Shader &tc, const Shader &te, const Shader &fs);
    RenderPipeline        pipeline(Topology tp,const RenderState& st, const Shader &vs, const Shader &gs, const Shader &fs);
    RenderPipeline        pipeline(const RenderState& st, const Shader &ts, const Shader &ms, const Shader &fs);

    ComputePipeline       pipeline(const Shader &comp);

    Fence                 fence();
    CommandBuffer         commandBuffer();

    const Builtin&        builtin() const;

  private:
    struct Impl {
      Impl(AbstractGraphicsApi& api, std::string_view name);
      Impl(AbstractGraphicsApi& api, DeviceType type);
      ~Impl();

      AbstractGraphicsApi&            api;
      AbstractGraphicsApi::Device*    dev=nullptr;
      };

    AbstractGraphicsApi&            api;
    Impl                            impl;
    AbstractGraphicsApi::Device*    dev=nullptr;
    Props                           devProps;
    Tempest::Builtin                builtins;

    Detail::VideoBuffer   createVideoBuffer(const void* data, size_t size, MemUsage usage, BufferHeap flg);
    RenderPipeline        implPipeline(const RenderState &st, const Shader* shaders[], Topology tp);
    template<class T>
    UniformBuffer<T>      implUbo(BufferHeap ht, const void* data);

    static TextureFormat  formatOf(const Attachment& a);

  friend class RenderPipeline;
  friend class Painter;
  friend class Shader;
  friend class CommandPool;
  friend class CommandBuffer;
  friend class DescriptorSet;

  friend class Texture2d;
  };

template<class T>
inline VertexBuffer<T> Device::vbo(BufferHeap ht, const T* arr, size_t arrSize) {
  if(arrSize==0)
    return VertexBuffer<T>();
  //if(sizeof(T)*arrSize>devProps.vbo.maxRange)
  //  throw std::system_error(Tempest::GraphicsErrc::TooLargeBuffer);

  static const auto   usageBits = MemUsage::VertexBuffer | MemUsage::StorageBuffer | MemUsage::TransferDst;
  Detail::VideoBuffer data      = createVideoBuffer(arr,arrSize*sizeof(T),usageBits,ht);
  VertexBuffer<T> vbo(std::move(data),arrSize);
  return vbo;
  }

template<class T>
inline IndexBuffer<T> Device::ibo(BufferHeap ht, const T* arr, size_t arrSize) {
  if(arrSize==0)
    return IndexBuffer<T>();

  static const auto   usageBits = MemUsage::IndexBuffer | MemUsage::StorageBuffer | MemUsage::TransferDst;
  Detail::VideoBuffer data      = createVideoBuffer(arr,arrSize*sizeof(T),usageBits,ht);
  IndexBuffer<T>    ibo(std::move(data),arrSize);
  return ibo;
  }

inline StorageBuffer Device::ssbo(BufferHeap ht, const void* data, size_t size) {
  if(size==0)
    return StorageBuffer();
  if(size>devProps.ssbo.maxRange)
    throw std::system_error(Tempest::GraphicsErrc::TooLargeBuffer);

  static const auto usageBits = MemUsage::VertexBuffer  | MemUsage::IndexBuffer   |
                                MemUsage::UniformBuffer | MemUsage::StorageBuffer |
                                MemUsage::TransferSrc   | MemUsage::TransferDst   |
                                MemUsage::Indirect      |
                                MemUsage::Initialized;
  Detail::VideoBuffer v = createVideoBuffer(data,size,usageBits,ht);
  return StorageBuffer(std::move(v));
  }

inline StorageBuffer Device::ssbo(BufferHeap ht, Uninitialized_t tag, size_t size) {
  if(size==0)
    return StorageBuffer();
  if(size>devProps.ssbo.maxRange)
    throw std::system_error(Tempest::GraphicsErrc::TooLargeBuffer);

  static const auto usageBits = MemUsage::VertexBuffer  | MemUsage::IndexBuffer   |
                                MemUsage::UniformBuffer | MemUsage::StorageBuffer |
                                MemUsage::TransferSrc   | MemUsage::TransferDst   |
                                MemUsage::Indirect;
  Detail::VideoBuffer v = createVideoBuffer(nullptr,size,usageBits,ht);
  return StorageBuffer(std::move(v));
  }

template<class T>
inline UniformBuffer<T> Device::implUbo(BufferHeap ht, const void* mem) {
  if(sizeof(T)>devProps.ubo.maxRange)
    throw std::system_error(Tempest::GraphicsErrc::TooLargeBuffer);

  Detail::VideoBuffer data = createVideoBuffer(mem,sizeof(T),MemUsage::UniformBuffer,ht);
  UniformBuffer<T> ubo(std::move(data));
  return ubo;
  }

template<class V, class I>
AccelerationStructure Device::blas(const VertexBuffer<V>& vbo, const IndexBuffer<I>& ibo) {
  RtGeometry g{vbo, ibo};
  return blas(&g, 1);
  }

template<class V, class I>
inline AccelerationStructure Device::blas(const VertexBuffer<V>& vbo, const IndexBuffer<I>& ibo, size_t offset, size_t count) {
  RtGeometry g{vbo, ibo, offset, count};
  return blas(&g, 1);
  }

}

