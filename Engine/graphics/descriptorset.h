#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/UniformBuffer>
#include <Tempest/StorageBuffer>
#include <Tempest/Except>

namespace Tempest {

class Device;
class CommandBuffer;
class Texture2d;
class Attachment;
class StorageImage;
class VideoBuffer;
class AccelerationStructure;

template<class T>
class Encoder;

class DescriptorSet final {
  public:
    DescriptorSet()=default;
    DescriptorSet(DescriptorSet&&);
    ~DescriptorSet();
    DescriptorSet& operator=(DescriptorSet&&);

    bool isEmpty() const { return impl.handler==nullptr; }

    template<class T>
    void set(size_t layoutBind, const UniformBuffer<T>& vbuf);
    template<class T>
    void set(size_t layoutBind, const UniformBuffer<T>& vbuf, size_t  offset);

    void set(size_t layoutBind, const StorageBuffer& vbuf);
    void set(size_t layoutBind, const StorageBuffer& vbuf, size_t  offset);

    void set(size_t layoutBind, const Texture2d&    tex, const Sampler2d& smp = Sampler2d::anisotrophy());
    void set(size_t layoutBind, const Attachment&   tex, const Sampler2d& smp = Sampler2d::anisotrophy());
    void set(size_t layoutBind, const StorageImage& tex, uint32_t mipLevel=0);
    void set(size_t layoutBind, const Detail::ResourcePtr<Texture2d>& tex, const Sampler2d& smp = Sampler2d::anisotrophy());

    void set(size_t layoutBind, const AccelerationStructure& tlas);

  private:
    struct EmptyDesc : AbstractGraphicsApi::Desc {
      void set    (size_t,AbstractGraphicsApi::Texture*, const Sampler2d&){}
      void setSsbo(size_t,AbstractGraphicsApi::Texture*, uint32_t){}
      void setUbo (size_t,AbstractGraphicsApi::Buffer*,  size_t){}
      void setSsbo(size_t,AbstractGraphicsApi::Buffer*,  size_t){}
      void ssboBarriers(Detail::ResourceState&){}
      };
    DescriptorSet(AbstractGraphicsApi::Desc* desc);
    void implBindUbo (size_t layoutBind, const VideoBuffer& vbuf, size_t offsetBytes);
    void implBindSsbo(size_t layoutBind, const VideoBuffer& vbuf, size_t offsetBytes);

    Detail::DPtr<AbstractGraphicsApi::Desc*> impl;
    static EmptyDesc emptyDesc;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

template<class T>
inline void DescriptorSet::set(size_t layoutBind,const UniformBuffer<T>& vbuf) {
  implBindUbo(layoutBind,vbuf.impl,0);
  }

template<class T>
inline void DescriptorSet::set(size_t layoutBind,const UniformBuffer<T>& vbuf, size_t offset) {
  if(offset!=0 && vbuf.alignedTSZ!=sizeof(T))
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer);
  implBindUbo(layoutBind,vbuf.impl,offset*vbuf.alignedTSZ);
  }

}
