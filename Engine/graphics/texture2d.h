#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class Uniforms;

//! Способы фильтрации текстуры.
enum class Filter : uint8_t {
  //! ближайшая фильтрация
  Nearest,
  //! линейная фильтрация
  Linear,
  //! Count
  Count
  };

enum class ClampMode : uint8_t {
  Clamp,
  ClampToBorder,
  ClampToEdge,
  MirroredRepeat,
  Repeat,
  Count
  };

//! simple 2d texture class
class Texture2d {
  public:
    struct Sampler {
      Filter    minFilter=Filter::Linear;
      Filter    magFilter=Filter::Linear;
      Filter    mipFilter=Filter::Linear;

      ClampMode uClamp   =ClampMode::Repeat;
      ClampMode vClamp   =ClampMode::Repeat;

      bool      anisotropic=true;

      void setClamping(ClampMode c){
        uClamp = c;
        vClamp = c;
        }

      void setFiltration(Filter f){
        minFilter = f;
        magFilter = f;
        mipFilter = f;
        }

      bool operator==(const Sampler& s) const {
        return minFilter==s.minFilter &&
               magFilter==s.magFilter &&
               mipFilter==s.mipFilter &&
               uClamp   ==s.uClamp    &&
               vClamp   ==s.vClamp    &&
               anisotropic==s.anisotropic;
        }

      bool operator!=(const Sampler& s) const {
        return !(*this==s);
        }
      };

    Texture2d()=default;
    Texture2d(Texture2d&&)=default;
    ~Texture2d();
    Texture2d& operator=(Texture2d&&)=default;

    int           w() const { return texW; }
    int           h() const { return texH; }
    bool          isEmpty() const { return texW<=0 || texH<=0; }
    TextureFormat format() const { return frm; }

    const Sampler& sampler() const { return smp; }
    void setSampler(const Sampler& s) { smp=s; }

  private:
    Texture2d(Tempest::Device& dev,AbstractGraphicsApi::Texture* impl,uint32_t w,uint32_t h,TextureFormat frm);

    Detail::DSharedPtr<AbstractGraphicsApi::Texture*> impl;
    int                                               texW=0;
    int                                               texH=0;
    Sampler                                           smp;
    TextureFormat                                     frm=RGBA8;

  friend class Tempest::Device;
  friend class Tempest::Uniforms;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}
