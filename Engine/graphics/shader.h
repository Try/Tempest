#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class HeadlessDevice;

class Shader final {
  public:
    Shader()=default;
    Shader(Shader&&)=default;
    ~Shader();
    Shader& operator=(Shader&&)=default;

    bool isEmpty() const { return impl.handler==nullptr; }

  private:
    Shader(Tempest::HeadlessDevice& dev,Detail::DSharedPtr<AbstractGraphicsApi::Shader*>&& impl);

    Tempest::HeadlessDevice*                         dev =nullptr;
    Detail::DSharedPtr<AbstractGraphicsApi::Shader*> impl;

  friend class Tempest::HeadlessDevice;
  };

}
