#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Texture2d>
#include "../utility/dptr.h"

namespace Tempest {

class StorageImage final {
  public:
    StorageImage() = default;
    StorageImage(StorageImage&&) = default;
    ~StorageImage() = default;
    StorageImage& operator=(StorageImage&&) = default;

    int           w()        const { return tImpl.w();        }
    int           h()        const { return tImpl.h();        }
    Size          size()     const { return tImpl.size();     }
    bool          isEmpty()  const { return tImpl.isEmpty();  }
    TextureFormat format()   const { return tImpl.format();   }
    uint32_t      mipCount() const { return tImpl.mipCount(); }

  private:
    StorageImage(Texture2d&& t):tImpl(std::move(t)){}

    Tempest::Texture2d tImpl;

  friend class Tempest::Device;
  friend class Tempest::DescriptorSet;
  friend class Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;

  friend       Texture2d& textureCast(StorageImage& s);
  friend const Texture2d& textureCast(const StorageImage& s);
  };

inline Texture2d& textureCast(StorageImage& s) {
  return s.tImpl;
  }

inline const Texture2d& textureCast(const StorageImage& s) {
  return s.tImpl;
  }
}
