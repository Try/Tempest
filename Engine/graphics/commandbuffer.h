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

template<class T>
class VertexBuffer;

class CommandBuffer {
  public:
    CommandBuffer(Tempest::Device& owner);
    CommandBuffer(CommandBuffer&& f)=default;
    ~CommandBuffer();
    CommandBuffer& operator = (CommandBuffer&& other)=default;

    void clear(Frame& fr,float r,float g,float b,float a);

    void begin();
    void end();

    void beginRenderPass(const FrameBuffer& fbo, const RenderPass& p, uint32_t width, uint32_t height);
    void endRenderPass();

    void setPipeline(Tempest::RenderPipeline& p);

    template<class T>
    void setVbo(const VertexBuffer<T>& vbo){ setVbo(vbo.impl); }

    void draw(size_t size);

  private:
    CommandBuffer(Tempest::Device& dev,AbstractGraphicsApi::CommandBuffer* f);

    void setVbo(const VideoBuffer &vbo);

    Tempest::Device*                                  dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::CommandBuffer*> impl;

  friend class Tempest::Device;
  };

}
