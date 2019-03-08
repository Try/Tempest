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

class VideoBuffer;
class Pixmap;

class Uniforms;
class UniformBuffer;
class UniformsLayout;

class Color;
class RenderState;

class Device {
  public:
    using Caps=AbstractGraphicsApi::Caps;

    Device(AbstractGraphicsApi& api,SystemApi::Window* w,uint8_t maxFramesInFlight=2);
    Device(const Device&)=delete;
    ~Device();

    void           waitIdle();
    void           reset();
    uint32_t       swapchainImageCount() const;
    uint8_t        maxFramesInFlight() const;
    uint8_t        frameId() const;
    uint64_t       frameCounter() const;
    uint32_t       imageId() const;

    Frame          frame(uint32_t id);
    uint32_t       nextImage(Semaphore& onReady);
    void           draw(const CommandBuffer&  cmd,const Semaphore& wait);
    void           draw(const CommandBuffer&  cmd,const Semaphore& wait,Semaphore& done,Fence& fdone);
    void           draw(const CommandBuffer*  cmd[],size_t count,const Semaphore& wait,Semaphore& done,Fence& fdone);
    void           present(uint32_t img,const Semaphore& wait);

    Shader         loadShader(const char* filename);
    Shader         loadShader(const char* source,const size_t length);

    const Caps&    caps() const;

    template<class T>
    VertexBuffer<T> loadVbo(const T* arr,size_t arrSize,BufferFlags flg);

    template<class T>
    VertexBuffer<T> loadVbo(const std::vector<T>& arr,BufferFlags flg){
      return loadVbo(arr.data(),arr.size(),flg);
      }

    template<class T>
    IndexBuffer<T>  loadIbo(const T* arr,size_t arrSize,BufferFlags flg);

    template<class T>
    VertexBuffer<T> loadIbo(const std::vector<T>& arr,BufferFlags flg){
      return loadIbo(arr.data(),arr.size(),flg);
      }

    Texture2d      createTexture(TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips);
    Texture2d      loadTexture(const Pixmap& pm,bool mips=true);
    UniformBuffer  loadUbo(const void* data, size_t size);

    Uniforms       uniforms(const UniformsLayout &owner);

    FrameBuffer    frameBuffer(Frame &out, RenderPass& pass);
    FrameBuffer    frameBuffer(Frame &out, Texture2d& zbuf, RenderPass& pass);

    RenderPass     pass       (FboMode color, FboMode zbuf, TextureFormat zbufFormat);
    RenderPass     pass       (const Tempest::Color& color);
    RenderPass     pass       (const Tempest::Color& color,FboMode zbuf, TextureFormat zbufFormat);
    RenderPass     pass       (const Tempest::Color& color,const float zbuf, TextureFormat zbufFormat);

    template<class Vertex>
    RenderPipeline pipeline(RenderPass& pass, uint32_t width, uint32_t height,
                            Topology tp,const RenderState& st,
                            const UniformsLayout& ulay,const Shader &vs,const Shader &fs);

    CommandBuffer  commandBuffer();
    CommandBuffer  commandSecondaryBuffer();

    const Builtin& builtin() const;

  private:
    struct Impl {
      Impl(AbstractGraphicsApi& api, SystemApi::Window *w,uint8_t maxFramesInFlight);
      ~Impl();

      AbstractGraphicsApi&            api;
      AbstractGraphicsApi::Device*    dev=nullptr;
      AbstractGraphicsApi::Swapchain* swapchain=nullptr;
      SystemApi::Window*              hwnd=nullptr;
      const uint8_t                   maxFramesInFlight=1;

      std::vector<AbstractGraphicsApi::CommandBuffer*> cmdBuf;
      };

    AbstractGraphicsApi&            api;
    Impl                            impl;
    AbstractGraphicsApi::Device*    dev=nullptr;
    AbstractGraphicsApi::Swapchain* swapchain=nullptr;
    Caps                            devCaps;

    Tempest::CommandPool            mainCmdPool;
    Tempest::Builtin                builtins;
    uint64_t                        framesCounter=0;
    uint8_t                         framesIdMod=0;
    uint32_t                        imgId=0;

    Fence       createFence();
    Semaphore   createSemaphore();
    VideoBuffer createVideoBuffer(const void* data, size_t size, MemUsage usage, BufferFlags flg);
    RenderPipeline
                implPipeline(RenderPass& pass, uint32_t width, uint32_t height,
                             const RenderState &st, const UniformsLayout& ulay,
                             const Shader &vs, const Shader &fs,
                             const Decl::ComponentType *decl, size_t declSize,
                             size_t stride, Topology tp);

    void       destroy(RenderPass&     p);
    void       destroy(FrameBuffer&    f);
    void       destroy(Shader&         s);
    void       destroy(Fence&          f);
    void       destroy(Semaphore&      s);
    void       destroy(CommandPool&    c);
    void       destroy(CommandBuffer&  c);
    void       destroy(VideoBuffer&    b);
    void       destroy(Uniforms&       u);

    void       begin(Frame& f,RenderPass& p);
    void       end  (Frame& f,RenderPass& p);

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
  friend class Texture2d;
  };

template<class T>
inline VertexBuffer<T> Device::loadVbo(const T* arr, size_t arrSize, BufferFlags flg) {
  if(arrSize==0)
    return VertexBuffer<T>();
  VideoBuffer     data=createVideoBuffer(arr,arrSize*sizeof(T),MemUsage::VertexBuffer,flg);
  VertexBuffer<T> vbo(std::move(data),arrSize);
  return vbo;
  }

template<class T>
inline IndexBuffer<T> Device::loadIbo(const T* arr, size_t arrSize, BufferFlags flg) {
  if(arrSize==0)
    return IndexBuffer<T>();
  VideoBuffer     data=createVideoBuffer(arr,arrSize*sizeof(T),MemUsage::IndexBuffer,flg);
  IndexBuffer<T>  ibo(std::move(data),arrSize);
  return ibo;
  }

template<class Vertex>
RenderPipeline Device::pipeline(RenderPass& pass, uint32_t width, uint32_t height,
                                Topology tp, const RenderState &st,
                                const UniformsLayout& ulay, const Shader &vs, const Shader &fs) {
  static const auto decl=Tempest::vertexBufferDecl<Vertex>();
  return implPipeline(pass,width,height,st,ulay,vs,fs,decl.begin(),decl.size(),sizeof(Vertex),tp);
  }

inline uint8_t Device::frameId() const {
  return framesIdMod;
  }
}
