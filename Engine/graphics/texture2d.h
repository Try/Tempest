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

    uint32_t w() const { return texW; }
    uint32_t h() const { return texH; }

  private:
    Texture2d(Tempest::Device& dev,AbstractGraphicsApi::Texture* impl,uint32_t w,uint32_t h);

    Tempest::Device*                                  dev =nullptr;
    Detail::DSharedPtr<AbstractGraphicsApi::Texture*> impl;
    uint32_t                                          texW=0;
    uint32_t                                          texH=0;

  friend class Tempest::Device;
  friend class Tempest::Uniforms;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}
