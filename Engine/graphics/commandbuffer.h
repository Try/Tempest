#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class Frame;
class RenderPass;
class FrameBuffer;
class RenderPipeline;
class VideoBuffer;
class Uniforms;
class Texture2d;

template<class T>
class VertexBuffer;

template<class T>
class IndexBuffer;

class CommandBuffer {
  public:
    CommandBuffer()=default;
    CommandBuffer(CommandBuffer&& f)=default;
    virtual ~CommandBuffer();
    CommandBuffer& operator = (CommandBuffer&& other)=default;

    void begin();
    void end();

    void setUniforms(const Tempest::RenderPipeline& p, const Tempest::Uniforms& ubo);
    void setUniforms(const Tempest::RenderPipeline& p, const Tempest::Uniforms& ubo, size_t offc, const uint32_t *offv);
    void setUniforms(const Detail::ResourcePtr<Tempest::RenderPipeline>& p,const Tempest::Uniforms& ubo);

    void setUniforms(const Tempest::RenderPipeline& p);
    void setUniforms(const Detail::ResourcePtr<Tempest::RenderPipeline>& p);

    void setViewport(int x,int y,int w,int h);
    void setViewport(const Rect& vp);

    template<class T>
    void draw(const VertexBuffer<T>& vbo){ implDraw(vbo.impl,0,vbo.size()); }

    template<class T>
    void draw(const VertexBuffer<T>& vbo,size_t offset,size_t count){ implDraw(vbo.impl,offset,count); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo,const IndexBuffer<I>& ibo){ implDraw(vbo.impl,ibo.impl,Detail::indexCls<I>(),0,ibo.size()); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo,const IndexBuffer<I>& ibo,size_t offset,size_t count)
         { implDraw(vbo.impl,ibo.impl,Detail::indexCls<I>(),offset,count); }

    void changeLayout(Texture2d& t,TextureLayout prev,TextureLayout next);

  private:
    CommandBuffer(Tempest::Device& dev,AbstractGraphicsApi::CommandBuffer* f,uint32_t vpWidth,uint32_t vpHeight);

    void implDraw(const VideoBuffer& vbo,size_t offset,size_t size);
    void implDraw(const VideoBuffer& vbo,const VideoBuffer& ibo,Detail::IndexClass index,size_t offset,size_t size);
    virtual void implEndRenderPass(){}

    const AbstractGraphicsApi::Pipeline* curPipeline=nullptr;
    const VideoBuffer*                   curVbo     =nullptr;
    const VideoBuffer*                   curIbo     =nullptr;

    enum Mode:uint8_t{
      Idle   = 0,
      Prime  = 1,
      Second = 2
      };

    struct Pass {
      const FrameBuffer* fbo   =nullptr;
      const RenderPass*  pass  =nullptr;

      Mode               mode=Idle;
      };

    struct Viewport {
      uint32_t width =0;
      uint32_t height=0;
      };

    Tempest::Device*                                  dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::CommandBuffer*> impl;
    Viewport                                          vp;

  friend class PrimaryCommandBuffer;
  friend class Tempest::Device;
  };

class PrimaryCommandBuffer : public CommandBuffer {
  public:
    void setPass(const FrameBuffer& fbo, const RenderPass& p);
    void setPass(const FrameBuffer& fbo, const RenderPass& p, int      width, int      height);
    void setPass(const FrameBuffer& fbo, const RenderPass& p, uint32_t width, uint32_t height);

    void exec(const FrameBuffer& fbo, const RenderPass& p, const CommandBuffer& buf);
    void exec(const FrameBuffer& fbo, const RenderPass& p,uint32_t width, uint32_t height,const CommandBuffer& buf);

  private:
    PrimaryCommandBuffer(Tempest::Device& dev,AbstractGraphicsApi::CommandBuffer* f):CommandBuffer(dev,f,0,0){}

    void implEndRenderPass();

    Pass curPass;

  friend class Tempest::Device;
  };
}
