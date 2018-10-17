#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Device>
#include "videobuffer.h"

namespace Tempest {

class Device;

class Uniforms {
  public:
    Uniforms()=default;
    Uniforms(Uniforms&&);
    ~Uniforms();
    Uniforms& operator=(Uniforms&&);

  private:
    Uniforms(Tempest::VideoBuffer&& impl,Tempest::Device& dev,AbstractGraphicsApi::Desc* desc);

    Tempest::Device*                         dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Desc*> desc;
    Tempest::VideoBuffer                     impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  };

}
