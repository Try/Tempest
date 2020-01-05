#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class Device;
class CommandBuffer;
class UniformBuffer;
class Texture2d;

template<class T>
class Encoder;

class Uniforms final {
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
    Uniforms(Tempest::Device& dev,AbstractGraphicsApi::Desc* desc);

    Tempest::Device*                         dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Desc*> desc;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

}
