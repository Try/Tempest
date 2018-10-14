#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;

class Shader {
  public:
    Shader()=default;
    Shader(Shader&&)=default;
    ~Shader();
    Shader& operator=(Shader&&)=default;

  private:
    Shader(Tempest::Device& dev,AbstractGraphicsApi::Shader* impl);

    Tempest::Device*                           dev =nullptr;
    Detail::DPtr<AbstractGraphicsApi::Shader*> impl;

  friend class Tempest::Device;
  };

}
