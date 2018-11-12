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

template<class T>
class VertexBuffer;

class CommandBuffer {
  public:
    CommandBuffer(CommandBuffer&& f)=default;
    ~CommandBuffer();
    CommandBuffer& operator = (CommandBuffer&& other)=default;

    void clear(Frame& fr,float r,float g,float b,float a);

    void begin();
    void end();

    void beginRenderPass(const FrameBuffer& fbo, const RenderPass& p, uint32_t width, uint32_t height);
    void endRenderPass();

    void setUniforms(const Tempest::RenderPipeline& p,const Tempest::Uniforms& ubo);

    template<class T>
    void draw(const VertexBuffer<T>& vbo,size_t count){ implDraw(vbo.impl,count); }

    void exec(const CommandBuffer& buf);

  private:
    CommandBuffer(Tempest::Device& dev,AbstractGraphicsApi::CommandBuffer* f);

    void implDraw(const VideoBuffer& vbo,size_t size);

    const RenderPipeline* curPipeline=nullptr;
    const VideoBuffer*    curVbo     =nullptr;

    Tempest::Device*                                  dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::CommandBuffer*> impl;

  friend class Tempest::Device;
  };

}
