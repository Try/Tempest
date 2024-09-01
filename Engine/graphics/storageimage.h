#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Texture2d>
#include "../utility/dptr.h"

namespace Tempest {

template<class T> T textureCast(StorageImage& s);
template<class T> T textureCast(const StorageImage& s);

class StorageImage final {
  public:
    StorageImage() = default;
    StorageImage(StorageImage&&) = default;
    ~StorageImage() = default;
    StorageImage& operator=(StorageImage&&) = default;

    int           w()        const { return tImpl.w();                 }
    int           h()        const { return tImpl.h();                 }
    int           d()        const { return tImpl.texD;                }
    Size          size()     const { return Size(tImpl.w(),tImpl.h()); }
    bool          isEmpty()  const { return tImpl.isEmpty();           }
    TextureFormat format()   const { return tImpl.format();            }
    uint32_t      mipCount() const { return tImpl.mipCount();          }

  private:
    StorageImage(Texture2d&& t):tImpl(std::move(t)){}

    Tempest::Texture2d tImpl;

  friend class Tempest::Device;
  friend class Tempest::DescriptorSet;
  friend class Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend T textureCast(StorageImage& s);

  template<class T>
  friend T textureCast(const StorageImage& s);
  };

template<>
inline Texture2d& textureCast<Texture2d&>(StorageImage& s) {
  if(s.d()>1)
    throw std::bad_cast();
  return s.tImpl;
  }

template<>
inline const Texture2d& textureCast<const Texture2d&>(StorageImage& s) {
  if(s.d()>1)
    throw std::bad_cast();
  return s.tImpl;
  }

template<>
inline const Texture2d& textureCast<const Texture2d&>(const StorageImage& s) {
  if(s.d()>1)
    throw std::bad_cast();
  return s.tImpl;
  }
}
