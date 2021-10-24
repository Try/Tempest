#pragma once

#include <Tempest/CommandBuffer>
#include <Tempest/RenderPipeline>
#include <Tempest/ComputePipeline>
#include <Tempest/DescriptorSet>

#include "videobuffer.h"

#include <vector>

namespace Tempest {

template<class T>
class VertexBuffer;

template<class T>
class IndexBuffer;

class CommandBuffer;

template<class T>
class Encoder;

template<>
class Encoder<Tempest::CommandBuffer> {
  public:
    Encoder(Encoder&& e);
    Encoder& operator = (Encoder&& e);
    virtual ~Encoder() noexcept(false);

    void setFramebuffer(std::initializer_list<AttachmentDesc> rd);
    void setFramebuffer(std::initializer_list<AttachmentDesc> rd, AttachmentDesc zd);
    void setFramebuffer(std::nullptr_t null);

    void setUniforms(const RenderPipeline& p, const DescriptorSet &ubo, const void* data, size_t sz);
    void setUniforms(const RenderPipeline& p, const void* data, size_t sz);
    void setUniforms(const RenderPipeline& p, const DescriptorSet &ubo);
    void setUniforms(const RenderPipeline& p);

    void setUniforms(const ComputePipeline& p, const DescriptorSet &ubo, const void* data, size_t sz);
    void setUniforms(const ComputePipeline& p, const void* data, size_t sz);
    void setUniforms(const ComputePipeline& p, const DescriptorSet &ubo);
    void setUniforms(const ComputePipeline& p);

    void setViewport(int x,int y,int w,int h);
    void setViewport(const Rect& vp);

    template<class T>
    void draw(const VertexBuffer<T>& vbo) { implDraw(vbo.impl,0,vbo.size(),0,1); }

    template<class T>
    void draw(const VertexBuffer<T>& vbo,size_t offset,size_t count) { implDraw(vbo.impl,offset,count,0,1); }

    template<class T>
    void draw(const VertexBuffer<T>& vbo,size_t offset,size_t count,size_t firstInstance,size_t instanceCount) { implDraw(vbo.impl,offset,count,firstInstance,instanceCount); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo,const IndexBuffer<I>& ibo)
         { implDraw(vbo.impl,ibo.impl,Detail::indexCls<I>(),0,ibo.size(),0,1); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo,const IndexBuffer<I>& ibo,size_t offset,size_t count)
         { implDraw(vbo.impl,ibo.impl,Detail::indexCls<I>(),offset,count,0,1); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo,const IndexBuffer<I>& ibo,size_t offset,size_t count,size_t firstInstance,size_t instanceCount)
         { implDraw(vbo.impl,ibo.impl,Detail::indexCls<I>(),offset,count,firstInstance,instanceCount); }

    void dispatch(size_t x, size_t y, size_t z);

    void copy(const Attachment& src, uint32_t mip, StorageBuffer& dest, size_t offset);
    void copy(const Texture2d&  src, uint32_t mip, StorageBuffer& dest, size_t offset);

    void generateMipmaps(Attachment& tex);

  private:
    Encoder(CommandBuffer* ow);

    enum Stage : uint8_t {
      None = 0,
      Rendering,
      Compute
      };

    struct State {
      const AbstractGraphicsApi::Pipeline*     curPipeline = nullptr;
      const AbstractGraphicsApi::CompPipeline* curCompute  = nullptr;
      Stage                                    stage       = None;
      };

    AbstractGraphicsApi::CommandBuffer* impl = nullptr;
    State                               state;

    void         implSetFramebuffer(const AttachmentDesc* rt, size_t rtSize, const AttachmentDesc* zs);
    void         implDraw(const VideoBuffer& vbo, size_t offset, size_t size, size_t firstInstance, size_t instanceCount);
    void         implDraw(const VideoBuffer &vbo, const VideoBuffer &ibo, Detail::IndexClass index,
                          size_t offset, size_t size, size_t firstInstance, size_t instanceCount);

  friend class CommandBuffer;
  };
}


