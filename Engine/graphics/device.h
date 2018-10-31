#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/CommandPool>
#include <Tempest/CommandBuffer>
#include <Tempest/RenderPass>
#include <Tempest/FrameBuffer>
#include <Tempest/RenderPipeline>
#include <Tempest/Shader>
#include <Tempest/Frame>
#include <Tempest/Uniforms>

#include "videobuffer.h"
#include <memory>

namespace Tempest {

class Fence;
class Semaphore;

class CommandPool;

class VideoBuffer;

template<class T>
class VertexBuffer;

class Uniforms;
class UniformsLayout;

class Device {
  public:
    Device(AbstractGraphicsApi& api,SystemApi::Window* w);
    Device(const Device&)=delete;
    ~Device();

    void           reset();
    uint32_t       swapchainImageCount() const;

    Frame          frame(uint32_t id);
    uint32_t       nextImage(Semaphore& onReady);
    void           draw(const CommandBuffer& cmd,const Semaphore& wait,Semaphore& done,Fence& fdone);
    void           present(uint32_t img,const Semaphore& wait);

    Shader         loadShader(const char* filename);

    template<class T>
    VertexBuffer<T> loadVbo(const T* arr,size_t arrSize,BufferFlags flg);

    Uniforms       loadUniforms(const void* data, size_t size, size_t count, const RenderPipeline& owner);

    FrameBuffer    frameBuffer(Frame &out, RenderPass& pass);
    RenderPass     pass    ();
    RenderPipeline pipeline(RenderPass& pass, uint32_t width, uint32_t height,const UniformsLayout& ulay,const Shader &vs,const Shader &fs);
    CommandBuffer  commandBuffer();

  private:
    struct Impl {
      Impl(AbstractGraphicsApi& api, SystemApi::Window *w);
      ~Impl();

      AbstractGraphicsApi&            api;
      AbstractGraphicsApi::Device*    dev=nullptr;
      AbstractGraphicsApi::Swapchain* swapchain=nullptr;
      SystemApi::Window*              hwnd=nullptr;
      };

    AbstractGraphicsApi&            api;
    Impl                            impl;
    AbstractGraphicsApi::Device*    dev=nullptr;
    AbstractGraphicsApi::Swapchain* swapchain=nullptr;

    Tempest::CommandPool            mainCmdPool;

    Fence      createFence();
    Semaphore  createSemaphore();
    VideoBuffer createVideoBuffer(const void* data, size_t size, MemUsage usage, BufferFlags flg);

    void       destroy(RenderPipeline& p);
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
  };

template<class T>
inline VertexBuffer<T> Device::loadVbo(const T* arr, size_t arrSize, BufferFlags flg) {
  VideoBuffer     data=createVideoBuffer(arr,arrSize*sizeof(T),MemUsage::VertexBuffer,flg);
  VertexBuffer<T> vbo(std::move(data));
  return vbo;
  }
}
