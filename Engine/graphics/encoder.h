#pragma once

#include <Tempest/CommandBuffer>
#include <Tempest/RenderPipeline>
#include <Tempest/Uniforms>

#include "videobuffer.h"

#include <vector>

namespace Tempest {

template<class T>
class VertexBuffer;

template<class T>
class IndexBuffer;

class CommandBuffer;
class PrimaryCommandBuffer;

class RenderPass;
class FrameBuffer;

template<class T>
class Encoder;

template<>
class Encoder<Tempest::CommandBuffer> {
  public:
    Encoder(Encoder&& e);
    Encoder& operator = (Encoder&& e);
    virtual ~Encoder() noexcept(false);

    void setUniforms(const Tempest::RenderPipeline& p, const Uniforms &ubo);
    void setUniforms(const Tempest::RenderPipeline& p, const Uniforms &ubo, size_t offc, const uint32_t* offv);
    void setUniforms(const Detail::ResourcePtr<RenderPipeline> &p, const Uniforms &ubo);
    void setUniforms(const Detail::ResourcePtr<RenderPipeline> &p);
    void setUniforms(const RenderPipeline &p);

    void setViewport(int x,int y,int w,int h);
    void setViewport(const Rect& vp);

    template<class T>
    void draw(const VertexBuffer<T>& vbo){ implDraw(vbo.impl,0,vbo.size()); }

    template<class T>
    void draw(const VertexBuffer<T>& vbo,size_t offset,size_t count){ implDraw(vbo.impl,offset,count); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo,const IndexBuffer<I>& ibo)
         { implDraw(vbo.impl,ibo.impl,Detail::indexCls<I>(),0,ibo.size()); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo,const IndexBuffer<I>& ibo,size_t offset,size_t count)
         { implDraw(vbo.impl,ibo.impl,Detail::indexCls<I>(),offset,count); }

  private:
    Encoder(CommandBuffer* ow);
    Encoder(AbstractGraphicsApi::CommandBuffer* impl);

    struct Viewport {
      uint32_t width =0;
      uint32_t height=0;
      };

    struct State {
      const AbstractGraphicsApi::Pipeline* curPipeline=nullptr;
      const VideoBuffer*                   curVbo     =nullptr;
      const VideoBuffer*                   curIbo     =nullptr;
      Viewport                             vp;
      };

    Tempest::CommandBuffer*             owner=nullptr;
    AbstractGraphicsApi::CommandBuffer* impl =nullptr;
    State                               state;

    void         implDraw(const VideoBuffer& vbo, size_t offset, size_t size);
    void         implDraw(const VideoBuffer &vbo, const VideoBuffer &ibo, Detail::IndexClass index,
                          size_t offset, size_t size);

  friend class CommandBuffer;
  friend class Encoder<Tempest::PrimaryCommandBuffer>;
  };

template<>
class Encoder<Tempest::PrimaryCommandBuffer> : public Encoder<Tempest::CommandBuffer> {
  public:
    Encoder(Encoder&& e);
    Encoder& operator = (Encoder&& e);
    virtual ~Encoder() noexcept(false);

    void setPass(const FrameBuffer& fbo, const RenderPass& p);
    void setPass(const FrameBuffer& fbo, const RenderPass& p, int      width, int      height);
    void setPass(const FrameBuffer& fbo, const RenderPass& p, uint32_t width, uint32_t height);

    void setLayout(Texture2d &t, TextureLayout dest);

    void exec(const CommandBuffer& buf);

  private:
    Encoder(PrimaryCommandBuffer* ow);

    struct ResState {
      AbstractGraphicsApi::Texture* res     = nullptr;
      TextureFormat                 frm     = TextureFormat::Undefined;
      TextureLayout                 next    = TextureLayout::Undefined;
      TextureLayout                 lay     = TextureLayout::Undefined;
      bool                          pending = false;
      };

    struct Pass {
      const FrameBuffer* fbo  = nullptr;
      const RenderPass*  pass = nullptr;
      };

    void      implEndRenderPass();
    ResState* findState(AbstractGraphicsApi::Texture* t);

    Pass                  curPass;
    std::vector<ResState> resState;

  friend class PrimaryCommandBuffer;
  };
}


