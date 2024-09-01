#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/UniformBuffer>
#include <Tempest/Except>

namespace Tempest {

class Device;
class CommandBuffer;
class Texture2d;
class StorageBuffer;
class StorageImage;
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

    void set(size_t layoutBind, const StorageBuffer& vbuf);
    void set(size_t layoutBind, const StorageBuffer& vbuf, size_t offset);

    void set(size_t layoutBind, const Texture2d&    tex, const Sampler& smp = Sampler::anisotrophy());
    void set(size_t layoutBind, const Attachment&   tex, const Sampler& smp = Sampler::anisotrophy());
    void set(size_t layoutBind, const ZBuffer&      tex, const Sampler& smp = Sampler::anisotrophy());
    void set(size_t layoutBind, const StorageImage& tex, const Sampler& smp = Sampler::anisotrophy(), uint32_t mipLevel=uint32_t(-1));

    void set(size_t layoutBind, const Sampler& smp);

    void set(size_t layoutBind, const Detail::ResourcePtr<Texture2d>& tex, const Sampler& smp = Sampler::anisotrophy());

    void set(size_t layoutBind, const std::vector<const Texture2d*>&     tex);
    void set(size_t layoutBind, const std::vector<const StorageBuffer*>& buf);

    void set(size_t layoutBind, const Texture2d*   const *   tex, size_t count);
    void set(size_t layoutBind, const StorageBuffer* const * buf, size_t count);

    void set(size_t layoutBind, const AccelerationStructure& tlas);

  private:
    DescriptorSet(AbstractGraphicsApi::Desc* desc);
    void implBindUbo (size_t layoutBind, const Detail::VideoBuffer& vbuf);
    void implBindSsbo(size_t layoutBind, const Detail::VideoBuffer& vbuf, size_t offset);

    Detail::DPtr<AbstractGraphicsApi::Desc*> impl;
    static AbstractGraphicsApi::EmptyDesc    emptyDesc;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

template<class T>
inline void DescriptorSet::set(size_t layoutBind, const UniformBuffer<T>& vbuf) {
  implBindUbo(layoutBind,vbuf.impl);
  }


}
