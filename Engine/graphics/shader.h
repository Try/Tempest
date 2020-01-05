#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;

class Shader final {
  public:
    Shader()=default;
    Shader(Shader&&)=default;
    ~Shader();
    Shader& operator=(Shader&&)=default;

    bool isEmpty() const { return impl.handler==nullptr; }

  private:
    Shader(Tempest::Device& dev,Detail::DSharedPtr<AbstractGraphicsApi::Shader*>&& impl);

    Tempest::Device*                                 dev = nullptr;
    Detail::DSharedPtr<AbstractGraphicsApi::Shader*> impl;

  friend class Tempest::Device;
  };

}
