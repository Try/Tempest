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
    ~CommandBuffer();
    CommandBuffer& operator = (CommandBuffer&& other)=default;

    void begin();//primal
    void begin(const RenderPass& p);//secondary
    void end();

    void setPass(const FrameBuffer& fbo, const RenderPass& p);
    void setPass(const FrameBuffer& fbo, const RenderPass& p, int      width, int      height);
    void setPass(const FrameBuffer& fbo, const RenderPass& p, uint32_t width, uint32_t height);

    void setSecondaryPass(const FrameBuffer& fbo, const RenderPass& p);
    void setSecondaryPass(const FrameBuffer& fbo, const RenderPass& p,uint32_t width, uint32_t height);

    void setUniforms(const Tempest::RenderPipeline& p, const Tempest::Uniforms& ubo);
    void setUniforms(const Tempest::RenderPipeline& p, const Tempest::Uniforms& ubo, size_t offc, const uint32_t *offv);
    void setUniforms(const Detail::ResourcePtr<Tempest::RenderPipeline>& p,const Tempest::Uniforms& ubo);

    void setUniforms(const Tempest::RenderPipeline& p);
    void setUniforms(const Detail::ResourcePtr<Tempest::RenderPipeline>& p);

    template<class T>
    void draw(const VertexBuffer<T>& vbo){ implDraw(vbo.impl,0,vbo.size()); }

    template<class T>
    void draw(const VertexBuffer<T>& vbo,size_t offset,size_t count){ implDraw(vbo.impl,offset,count); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo,const IndexBuffer<I>& ibo){ implDraw(vbo.impl,ibo.impl,Detail::indexCls<I>(),0,ibo.size()); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo,const IndexBuffer<I>& ibo,size_t offset,size_t count)
         { implDraw(vbo.impl,ibo.impl,Detail::indexCls<I>(),offset,count); }

    void exec(const CommandBuffer& buf);

  private:
    CommandBuffer(Tempest::Device& dev,AbstractGraphicsApi::CommandBuffer* f);

    void implDraw(const VideoBuffer& vbo,size_t offset,size_t size);
    void implDraw(const VideoBuffer& vbo,const VideoBuffer& ibo,Detail::IndexClass index,size_t offset,size_t size);

    void implEndRenderPass();

    const AbstractGraphicsApi::Pipeline* curPipeline=nullptr;
    const VideoBuffer*                   curVbo     =nullptr;
    const VideoBuffer*                   curIbo     =nullptr;

    struct Pass {
      const FrameBuffer* fbo   =nullptr;
      const RenderPass*  pass  =nullptr;
      uint32_t           width =0;
      uint32_t           height=0;

      bool               active=false;
      };
    Pass curPass;

    Tempest::Device*                                  dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::CommandBuffer*> impl;

  friend class Tempest::Device;
  };

}
