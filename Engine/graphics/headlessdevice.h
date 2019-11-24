#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/CommandPool>
#include <Tempest/CommandBuffer>
#include <Tempest/RenderPass>
#include <Tempest/FrameBuffer>
#include <Tempest/RenderPipeline>
#include <Tempest/Shader>
#include <Tempest/Frame>
#include <Tempest/Texture2d>
#include <Tempest/Uniforms>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Builtin>

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
class UniformBuffer;
class UniformsLayout;

class Color;
class RenderState;

class HeadlessDevice {
  public:
    using Caps=AbstractGraphicsApi::Caps;

    HeadlessDevice(AbstractGraphicsApi& api);
    HeadlessDevice(const Device&)=delete;
    virtual ~HeadlessDevice();

    void                 waitIdle();

    void                 draw(const PrimaryCommandBuffer&  cmd,const Semaphore& wait);
    void                 draw(const PrimaryCommandBuffer&  cmd,const Semaphore& wait,Semaphore& done,Fence& fdone);
    void                 draw(const PrimaryCommandBuffer *cmd[], size_t count,
                              const Semaphore* wait[], size_t waitCnt,
                              Semaphore* done[], size_t doneCnt,
                              Fence* fdone);

    Shader               loadShader(RFile&          file);
    Shader               loadShader(const char*     filename);
    Shader               loadShader(const char16_t* filename);
    Shader               loadShader(const char* source,const size_t length);

    const Caps&          caps() const;

    template<class T>
    VertexBuffer<T>      loadVbo(const T* arr,size_t arrSize,BufferFlags flg);

    template<class T>
    VertexBuffer<T>      loadVbo(const std::vector<T>& arr,BufferFlags flg){
      return loadVbo(arr.data(),arr.size(),flg);
      }

    template<class T>
    VertexBufferDyn<T>   loadVboDyn(const T* arr,size_t arrSize);

    template<class T>
    VertexBufferDyn<T>   loadVboDyn(const std::vector<T>& arr){
      return loadVboDyn(arr.data(),arr.size());
      }

    template<class T>
    IndexBuffer<T>       loadIbo(const T* arr,size_t arrSize,BufferFlags flg);

    template<class T>
    VertexBuffer<T>      loadIbo(const std::vector<T>& arr,BufferFlags flg){
      return loadIbo(arr.data(),arr.size(),flg);
      }

    Texture2d            texture(TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips);
    Texture2d            loadTexture(const Pixmap& pm,bool mips=true);
    UniformBuffer        loadUbo(const void* data, size_t size);
    Pixmap               readPixels(const Texture2d& t);

    Uniforms             uniforms(const UniformsLayout &owner);

    FrameBuffer          frameBuffer(Texture2d &out);
    FrameBuffer          frameBuffer(Texture2d &out, Texture2d& zbuf);

    RenderPass           pass(const Attachment& color);
    RenderPass           pass(const Attachment& color,const Attachment& depth);

    template<class Vertex>
    RenderPipeline       pipeline(Topology tp,const RenderState& st,
                                  const UniformsLayout& ulay,const Shader &vs,const Shader &fs);

    PrimaryCommandBuffer commandBuffer();
    CommandBuffer        commandSecondaryBuffer(const FrameBufferLayout &lay);
    CommandBuffer        commandSecondaryBuffer(const FrameBuffer &fbo);

    const Builtin&       builtin() const;
    const char*          renderer() const;

  protected:
    HeadlessDevice(AbstractGraphicsApi& api, SystemApi::Window *window);
    AbstractGraphicsApi::Device* implHandle();

  private:
    struct Impl {
      Impl(AbstractGraphicsApi& api,SystemApi::Window *w);
      ~Impl();

      AbstractGraphicsApi&            api;
      AbstractGraphicsApi::Device*    dev=nullptr;
      };

    AbstractGraphicsApi&            api;
    Impl                            impl;
    AbstractGraphicsApi::Device*    dev=nullptr;
    Caps                            devCaps;
    Tempest::CommandPool            mainCmdPool;
    Tempest::Builtin                builtins;

    Fence       createFence();
    Semaphore   createSemaphore();
    VideoBuffer createVideoBuffer(const void* data, size_t size, MemUsage usage, BufferFlags flg);
    RenderPipeline
                implPipeline(const RenderState &st, const UniformsLayout& ulay,
                             const Shader &vs, const Shader &fs,
                             const Decl::ComponentType *decl, size_t declSize,
                             size_t stride, Topology tp);
    void        implDraw(const Tempest::PrimaryCommandBuffer *cmd[], AbstractGraphicsApi::CommandBuffer* hcmd[],  size_t count,
                         const Semaphore* wait[], AbstractGraphicsApi::Semaphore*     hwait[], size_t waitCnt,
                          Semaphore*      done[], AbstractGraphicsApi::Semaphore*     hdone[], size_t doneCnt,
                         AbstractGraphicsApi::Fence*         fdone);

    void       destroy(Uniforms&       u);

  friend class RenderPipeline;
  friend class RenderPass;
  friend class FrameBuffer;
  friend class Painter;
  friend class Shader;
  friend class Fence;
  friend class Semaphore;
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
inline VertexBuffer<T> HeadlessDevice::loadVbo(const T* arr, size_t arrSize, BufferFlags flg) {
  if(arrSize==0)
    return VertexBuffer<T>();
  VideoBuffer     data=createVideoBuffer(arr,arrSize*sizeof(T),MemUsage::VertexBuffer,flg);
  VertexBuffer<T> vbo(std::move(data),arrSize);
  return vbo;
  }

template<class T>
inline VertexBufferDyn<T> HeadlessDevice::loadVboDyn(const T *arr, size_t arrSize) {
  if(arrSize==0)
    return VertexBufferDyn<T>();
  VideoBuffer        data=createVideoBuffer(arr,arrSize*sizeof(T),MemUsage::VertexBuffer,BufferFlags::Dynamic);
  VertexBufferDyn<T> vbo(std::move(data),arrSize);
  return vbo;
  }

template<class T>
inline IndexBuffer<T> HeadlessDevice::loadIbo(const T* arr, size_t arrSize, BufferFlags flg) {
  if(arrSize==0)
    return IndexBuffer<T>();
  VideoBuffer     data=createVideoBuffer(arr,arrSize*sizeof(T),MemUsage::IndexBuffer,flg);
  IndexBuffer<T>  ibo(std::move(data),arrSize);
  return ibo;
  }

template<class Vertex>
RenderPipeline HeadlessDevice::pipeline(Topology tp, const RenderState &st,
                                        const UniformsLayout& ulay, const Shader &vs, const Shader &fs) {
  static const auto decl=Tempest::vertexBufferDecl<Vertex>();
  return implPipeline(st,ulay,vs,fs,decl.begin(),decl.size(),sizeof(Vertex),tp);
  }

}

