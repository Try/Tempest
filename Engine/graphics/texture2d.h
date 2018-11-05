#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class Uniforms;

class Texture2d {
  public:
    Texture2d()=default;
    Texture2d(Texture2d&&)=default;
    ~Texture2d();
    Texture2d& operator=(Texture2d&&)=default;

  private:
    Texture2d(Tempest::Device& dev,AbstractGraphicsApi::Texture* impl);

    Tempest::Device*                            dev =nullptr;
    Detail::DPtr<AbstractGraphicsApi::Texture*> impl;

  friend class Tempest::Device;
  friend class Tempest::Uniforms;
  };

}
