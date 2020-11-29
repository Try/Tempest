#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/UniformBuffer>
#include <Tempest/StorageBuffer>

namespace Tempest {

class Device;
class CommandBuffer;
class Texture2d;
class Attachment;
class StorageImage;
class VideoBuffer;

template<class T>
class Encoder;

class Uniforms final {
  public:
    Uniforms()=default;
    Uniforms(Uniforms&&);
    ~Uniforms();
    Uniforms& operator=(Uniforms&&);

    bool isEmpty() const { return dev==nullptr; }

    template<class T>
    void set(size_t layoutBind,const UniformBuffer<T>& vbuf);
    template<class T>
    void set(size_t layoutBind,const UniformBuffer<T>& vbuf,size_t offset,size_t size);

    template<class T>
    void set(size_t layoutBind,const StorageBuffer<T>& vbuf);
    template<class T>
    void set(size_t layoutBind,const StorageBuffer<T>& vbuf,size_t offset,size_t size);

    void set(size_t layoutBind, const Texture2d&    tex, const Sampler2d& smp = Sampler2d::anisotrophy());
    void set(size_t layoutBind, const Attachment&   tex, const Sampler2d& smp = Sampler2d::anisotrophy());
    void set(size_t layoutBind, const StorageImage& tex, uint32_t mipLevel=0);
    void set(size_t layoutBind, const Detail::ResourcePtr<Texture2d>& tex, const Sampler2d& smp = Sampler2d::anisotrophy());

  private:
    Uniforms(Tempest::Device& dev,AbstractGraphicsApi::Desc* desc);
    void implBindUbo (size_t layoutBind, const VideoBuffer& vbuf, size_t offset, size_t count, size_t size);
    void implBindSsbo(size_t layoutBind, const VideoBuffer& vbuf, size_t offset, size_t count, size_t size);

    Tempest::Device*                         dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Desc*> desc;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

template<class T>
inline void Uniforms::set(size_t layoutBind,const UniformBuffer<T>& vbuf) {
  implBindUbo(layoutBind,vbuf.impl,0,vbuf.size(),vbuf.alignedTSZ);
  }

template<class T>
inline void Uniforms::set(size_t layoutBind, const UniformBuffer<T>& vbuf, size_t offset, size_t size) {
  implBindUbo(layoutBind,vbuf.impl,offset,size,vbuf.alignedTSZ);
  }

template<class T>
inline void Uniforms::set(size_t layoutBind,const StorageBuffer<T>& vbuf) {
  implBindSsbo(layoutBind,vbuf.impl,0,vbuf.size(),vbuf.alignedTSZ);
  }

template<class T>
inline void Uniforms::set(size_t layoutBind, const StorageBuffer<T>& vbuf, size_t offset, size_t size) {
  implBindSsbo(layoutBind,vbuf.impl,offset,size,vbuf.alignedTSZ);
  }
}
