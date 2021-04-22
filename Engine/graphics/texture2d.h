#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class DescriptorSet;
template<class T>
class Encoder;

class CommandBuffer;

//! simple 2d texture class
class Texture2d final {
  public:
    Texture2d()=default;
    Texture2d(Texture2d&&);
    ~Texture2d();
    Texture2d& operator=(Texture2d&&);

    int           w() const { return texW; }
    int           h() const { return texH; }
    Size          size() const { return Size(texW,texH); }
    bool          isEmpty() const { return texW<=0 || texH<=0; }
    TextureFormat format() const { return frm; }
    uint32_t      mipCount() const;

  private:
    Texture2d(Tempest::Device& dev,AbstractGraphicsApi::PTexture&& impl,uint32_t w,uint32_t h,TextureFormat frm);

    Detail::DSharedPtr<AbstractGraphicsApi::Texture*> impl;
    int                                               texW=0;
    int                                               texH=0;
    TextureFormat                                     frm=Undefined;

  friend class Tempest::Device;
  friend class Tempest::DescriptorSet;
  friend class Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}
