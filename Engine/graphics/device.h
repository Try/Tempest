#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/CommandBuffer>
#include <Tempest/RenderPass>
#include <Tempest/FrameBuffer>
#include <Tempest/RenderPipeline>
#include <Tempest/ComputePipeline>
#include <Tempest/Shader>
#include <Tempest/Attachment>
#include <Tempest/ZBuffer>
#include <Tempest/Texture2d>
#include <Tempest/Uniforms>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/StorageBuffer>
#include <Tempest/Builtin>
#include <Tempest/Swapchain>
#include <Tempest/UniformBuffer>
#include <Tempest/Except>

#include "videobuffer.h"

#include <memory>
#include <vector>

namespace Tempest {

class Fence;
class Semaphore;

class CommandPool;
class RFile;

class VideoBuffer;
class Pixmap;

class Uniforms;
class UniformsLayout;

class Color;
class RenderState;

class Device {
  public:
    using Props=AbstractGraphicsApi::Props;

    Device(AbstractGraphicsApi& api, uint8_t maxFramesInFlight=2);
    Device(AbstractGraphicsApi& api, const char* name, uint8_t maxFramesInFlight=2);
    Device(const Device&)=delete;
    virtual ~Device();

    uint8_t              maxFramesInFlight() const;
    void                 waitIdle();

    void                 submit(const CommandBuffer&  cmd,const Semaphore& wait);
    void                 submit(const CommandBuffer&  cmd,Fence& fdone);
    void                 submit(const CommandBuffer&  cmd,const Semaphore& wait,Semaphore& done,Fence& fdone);
    void                 submit(const CommandBuffer *cmd[], size_t count,
                                const Semaphore* wait[], size_t waitCnt,
                                Semaphore* done[], size_t doneCnt,
                                Fence* fdone);
    void                 present(Swapchain& sw, uint32_t img, const Semaphore& wait);

    Swapchain            swapchain(SystemApi::Window* w) const;

    Shader               loadShader(RFile&          file);
    Shader               loadShader(const char*     filename);
    Shader               loadShader(const char16_t* filename);
    Shader               shader    (const void* source, const size_t length);

    const Props&         properties() const;

    template<class T>
    VertexBuffer<T>      vbo(const T* arr,size_t arrSize);

    template<class T>
    VertexBuffer<T>      vbo(const std::vector<T>& arr){
      return vbo(arr.data(),arr.size());
      }

    template<class T>
    VertexBufferDyn<T>   vboDyn(const T* arr,size_t arrSize);

    template<class T>
    VertexBufferDyn<T>   vboDyn(const std::vector<T>& arr){
      return vboDyn(arr.data(),arr.size());
      }

    template<class T>
    IndexBuffer<T>       ibo(const T* arr,size_t arrSize);

    template<class T>
    IndexBuffer<T>       ibo(const std::vector<T>& arr){
      return ibo(arr.data(),arr.size());
      }

    template<class T>
    UniformBuffer<T>     ubo(const T* data, size_t size);

    template<class T>
    UniformBuffer<T>     ubo(const T& data);

    template<class T>
    StorageBuffer<T>     ssbo(const T* arr,size_t arrSize);

    template<class T>
    StorageBuffer<T>     ssbo(const std::vector<T>& arr){
      return ssbo(arr.data(),arr.size());
      }

    Uniforms             uniforms(const UniformsLayout &owner);

    Attachment           attachment (TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips = false);
    ZBuffer              zbuffer    (TextureFormat frm, const uint32_t w, const uint32_t h);
    Texture2d            loadTexture(const Pixmap& pm,bool mips=true);
    Pixmap               readPixels (const Texture2d&  t);
    Pixmap               readPixels (const Attachment& t);

    FrameBuffer          frameBuffer(Attachment& out);
    FrameBuffer          frameBuffer(Attachment& out, ZBuffer& zbuf);
    FrameBuffer          frameBuffer(Attachment& out0, Attachment& out1, ZBuffer& zbuf);
    FrameBuffer          frameBuffer(Attachment& out0, Attachment& out1, Attachment& out2, ZBuffer& zbuf);
    FrameBuffer          frameBuffer(Attachment& out0, Attachment& out1, Attachment& out2, Attachment& out3, ZBuffer& zbuf);
    FrameBuffer          frameBuffer(Attachment** out, uint8_t count, ZBuffer* zbuf);

    RenderPass           pass(const FboMode& color);
    RenderPass           pass(const FboMode& color, const FboMode& depth);
    RenderPass           pass(const FboMode& c0, const FboMode& c1, const FboMode& depth);
    RenderPass           pass(const FboMode& c0, const FboMode& c1, const FboMode& c2, const FboMode& depth);
    RenderPass           pass(const FboMode& c0, const FboMode& c1, const FboMode& c2, const FboMode& c3, const FboMode& depth);
    RenderPass           pass(const FboMode** color, uint8_t count);

    template<class Vertex>
    RenderPipeline       pipeline(Topology tp,const RenderState& st, const Shader &vs,const Shader &fs);

    ComputePipeline      pipeline(const Shader &comp);

    Fence                fence();
    Semaphore            semaphore();

    CommandBuffer        commandBuffer();

    const Builtin&       builtin() const;
    const char*          renderer() const;

  private:
    struct Impl {
      Impl(AbstractGraphicsApi& api, const char* name, uint8_t maxFramesInFlight);
      ~Impl();

      AbstractGraphicsApi&            api;
      AbstractGraphicsApi::Device*    dev=nullptr;
      uint8_t                         maxFramesInFlight=1;
      };

    AbstractGraphicsApi&            api;
    Impl                            impl;
    AbstractGraphicsApi::Device*    dev=nullptr;
    Props                           devProps;
    Tempest::Builtin                builtins;

    VideoBuffer createVideoBuffer(const void* data, size_t count, size_t size, size_t alignedSz, MemUsage usage, BufferHeap flg);

    RenderPipeline
                implPipeline(const RenderState &st,
                             const Shader &vs, const Shader &fs,
                             const Decl::ComponentType *decl, size_t declSize,
                             size_t stride, Topology tp);
    void        implSubmit(const Tempest::CommandBuffer *cmd[], AbstractGraphicsApi::CommandBuffer* hcmd[],  size_t count,
                           const Semaphore* wait[], AbstractGraphicsApi::Semaphore*     hwait[], size_t waitCnt,
                           Semaphore*       done[], AbstractGraphicsApi::Semaphore*     hdone[], size_t doneCnt,
                           AbstractGraphicsApi::Fence*         fdone);

    static TextureFormat formatOf(const Attachment& a);

  friend class RenderPipeline;
  friend class RenderPass;
  friend class FrameBuffer;
  friend class Painter;
  friend class Shader;
  friend class CommandPool;
  friend class CommandBuffer;
  friend class VideoBuffer;
  friend class Uniforms;

  template<class T>
  friend class VertexBuffer;
  template<class T>
  friend class VertexBufferDyn;

  friend class Texture2d;
  };

template<class T>
inline VertexBuffer<T> Device::vbo(const T* arr, size_t arrSize) {
  if(arrSize==0)
    return VertexBuffer<T>();
  VideoBuffer     data=createVideoBuffer(arr,arrSize,sizeof(T),sizeof(T),MemUsage::VertexBuffer,BufferHeap::Static);
  VertexBuffer<T> vbo(std::move(data),arrSize);
  return vbo;
  }

template<class T>
inline VertexBufferDyn<T> Device::vboDyn(const T *arr, size_t arrSize) {
  if(arrSize==0)
    return VertexBufferDyn<T>();
  VideoBuffer        data=createVideoBuffer(arr,arrSize,sizeof(T),sizeof(T),MemUsage::VertexBuffer,BufferHeap::Upload);
  VertexBufferDyn<T> vbo(std::move(data),arrSize);
  return vbo;
  }

template<class T>
inline IndexBuffer<T> Device::ibo(const T* arr, size_t arrSize) {
  if(arrSize==0)
    return IndexBuffer<T>();
  VideoBuffer     data=createVideoBuffer(arr,arrSize,sizeof(T),sizeof(T),MemUsage::IndexBuffer,BufferHeap::Static);
  IndexBuffer<T>  ibo(std::move(data),arrSize);
  return ibo;
  }

template<class T>
inline StorageBuffer<T> Device::ssbo(const T* arr, size_t arrSize) {
  if(arrSize==0)
    return StorageBuffer<T>();
  static const auto usageBits = MemUsage::VertexBuffer  | MemUsage::IndexBuffer   |
                                MemUsage::UniformBuffer | MemUsage::StorageBuffer |
                                MemUsage::TransferSrc   | MemUsage::TransferDst;
  VideoBuffer       data      = createVideoBuffer(arr,arrSize,sizeof(T),sizeof(T),usageBits,BufferHeap::Static);
  StorageBuffer<T>  sbo(std::move(data),arrSize);
  return sbo;
  }

template<class T>
inline UniformBuffer<T> Device::ubo(const T *mem, size_t size) {
  if(size==0)
    return UniformBuffer<T>();
  const size_t align   = devProps.ubo.offsetAlign;
  const size_t eltSize = ((sizeof(T)+align-1)/align)*align;

  if(sizeof(T)>devProps.ubo.maxRange)
    throw std::system_error(Tempest::GraphicsErrc::TooLardgeUbo);
  VideoBuffer      data=createVideoBuffer(mem,size,sizeof(T),eltSize,MemUsage::UniformBuffer,BufferHeap::Upload);
  UniformBuffer<T> ubo(std::move(data),eltSize);
  return ubo;
  }

template<class T>
inline UniformBuffer<T> Device::ubo(const T& mem) {
  return ubo(&mem,1);
  }

template<class Vertex>
RenderPipeline Device::pipeline(Topology tp, const RenderState &st, const Shader &vs, const Shader &fs) {
  static const auto decl=Tempest::vertexBufferDecl<Vertex>();
  return implPipeline(st,vs,fs,decl.data.data(),decl.data.size(),sizeof(Vertex),tp);
  }

}

