#pragma once

#include <Tempest/CommandBuffer>
#include <Tempest/RenderPipeline>
#include <Tempest/ComputePipeline>
#include <Tempest/DescriptorSet>

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

    void setScissor(int x,int y,int w,int h);
    void setScissor(const Rect& vp);

    void setDebugMarker(std::string_view tag);

    // non-indexed + empty vbo
    void draw(std::nullptr_t vbo, size_t offset, size_t count) { implDraw({},0,offset,count,0,1); }
    void draw(std::nullptr_t vbo, size_t offset, size_t count, size_t firstInstance, size_t instanceCount) { implDraw({},0,offset,count,firstInstance,instanceCount); }

    // non-indexed
    template<class T>
    void draw(const VertexBuffer<T>& vbo) { implDraw(vbo.impl,sizeof(T),0,vbo.size(),0,1); }

    template<class T>
    void draw(const VertexBuffer<T>& vbo, size_t offset, size_t count) { implDraw(vbo.impl,sizeof(T),offset,count,0,1); }

    template<class T>
    void draw(const VertexBuffer<T>& vbo, size_t offset, size_t count, size_t firstInstance, size_t instanceCount) { implDraw(vbo.impl,sizeof(T),offset,count,firstInstance,instanceCount); }

    // indexed + empty vbo
    template<class I>
    void draw(std::nullptr_t vbo, const IndexBuffer<I>& ibo)
      { implDraw({},0,ibo.impl,Detail::indexCls<I>(),0,ibo.size(),0,1); }

    template<class I>
    void draw(std::nullptr_t vbo, const IndexBuffer<I>& ibo, size_t offset, size_t count)
      { implDraw({},0,ibo.impl,Detail::indexCls<I>(),offset,count,0,1); }

    template<class I>
    void draw(std::nullptr_t vbo, const IndexBuffer<I>& ibo, size_t offset, size_t count, size_t firstInstance, size_t instanceCount)
      { implDraw({},0,ibo.impl,Detail::indexCls<I>(),offset,count,firstInstance,instanceCount); }

    // indexed
    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo, const IndexBuffer<I>& ibo)
         { implDraw(vbo.impl,sizeof(T),ibo.impl,Detail::indexCls<I>(),0,ibo.size(),0,1); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo, const IndexBuffer<I>& ibo, size_t offset, size_t count)
         { implDraw(vbo.impl,sizeof(T),ibo.impl,Detail::indexCls<I>(),offset,count,0,1); }

    template<class T,class I>
    void draw(const VertexBuffer<T>& vbo, const IndexBuffer<I>& ibo, size_t offset, size_t count, size_t firstInstance, size_t instanceCount)
         { implDraw(vbo.impl,sizeof(T),ibo.impl,Detail::indexCls<I>(),offset,count,firstInstance,instanceCount); }

    void drawIndirect(const StorageBuffer& indirect, size_t offset);

    void dispatchMesh(size_t x, size_t y=1, size_t z=1);
    void dispatchMeshIndirect(const StorageBuffer& indirect, size_t offset);
    void dispatchMeshThreads(size_t x, size_t y=1, size_t z=1);
    void dispatchMeshThreads(Size sz);

    void dispatch(size_t x, size_t y=1, size_t z=1);
    void dispatchIndirect(const StorageBuffer& indirect, size_t offset);
    void dispatchThreads(size_t x, size_t y=1, size_t z=1);
    void dispatchThreads(Size sz);

    void copy(const Attachment& src, uint32_t mip, StorageBuffer& dest, size_t offset);
    void copy(const Texture2d&  src, uint32_t mip, StorageBuffer& dest, size_t offset);

    void generateMipmaps(Attachment& tex);

  private:
    explicit Encoder(CommandBuffer* ow);

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
    void         implDraw(const Detail::VideoBuffer& vbo, size_t stride, size_t offset, size_t size, size_t firstInstance, size_t instanceCount);
    void         implDraw(const Detail::VideoBuffer& vbo, size_t stride, const Detail::VideoBuffer &ibo, Detail::IndexClass index,
                          size_t offset, size_t size, size_t firstInstance, size_t instanceCount);

  friend class CommandBuffer;
  };
}


