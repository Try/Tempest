#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Device>
#include "videobuffer.h"

namespace Tempest {

class Device;

class Uniforms {
  public:
    Uniforms()=default;
    Uniforms(Uniforms&&)=default;
    Uniforms& operator=(Uniforms&&)=default;

  private:
    Uniforms(Tempest::VideoBuffer&& impl);

    Tempest::VideoBuffer impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  };

}
