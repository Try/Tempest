#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Device>

namespace Tempest {

class Device;
class UniformBuffer;

class Uniforms {
  public:
    Uniforms()=default;
    Uniforms(Uniforms&&);
    ~Uniforms();
    Uniforms& operator=(Uniforms&&);

    void set(size_t layoutBind,const UniformBuffer& vbuf,size_t offset,size_t size);
    void set(size_t layoutBind,const Texture2d& tex);

  private:
    Uniforms(Tempest::Device& dev,AbstractGraphicsApi::Desc* desc);

    Tempest::Device*                         dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Desc*> desc;
    //Tempest::VideoBuffer                     impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  };

}
