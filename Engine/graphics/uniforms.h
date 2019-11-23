#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class HeadlessDevice;
class CommandBuffer;
class UniformBuffer;
class Texture2d;

template<class T>
class Encoder;

class Uniforms {
  public:
    Uniforms()=default;
    Uniforms(Uniforms&&);
    ~Uniforms();
    Uniforms& operator=(Uniforms&&);

    bool isEmpty() const { return dev==nullptr; }

    void set(size_t layoutBind,const UniformBuffer& vbuf);
    void set(size_t layoutBind,const UniformBuffer& vbuf,size_t offset,size_t size);
    void set(size_t layoutBind,const Texture2d& tex);
    void set(size_t layoutBind,const Detail::ResourcePtr<Texture2d>& tex);

  private:
    Uniforms(Tempest::HeadlessDevice& dev,AbstractGraphicsApi::Desc* desc);

    Tempest::HeadlessDevice*                 dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Desc*> desc;

  friend class Tempest::HeadlessDevice;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

}
