#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Texture2d>

namespace Tempest {

class Device;
class CommandBuffer;
class Texture2d;

//! attachment 2d texture class
class ZBuffer final {
  public:
    ZBuffer()=default;
    ZBuffer(ZBuffer&&)=default;
    ~ZBuffer()=default;
    ZBuffer& operator=(ZBuffer&&)=default;

    int              w()       const { return tImpl.w();       }
    int              h()       const { return tImpl.h();       }
    Size             size()    const { return tImpl.size();    }
    bool             isEmpty() const { return tImpl.isEmpty(); }

    friend       Texture2d& textureCast(ZBuffer& a);
    friend const Texture2d& textureCast(const ZBuffer& a);

  private:
    ZBuffer(Texture2d&& t, bool sampleFormat):tImpl(std::move(t)), sampleFormat(sampleFormat) {}

    Texture2d tImpl;
    bool      sampleFormat=false;

  friend class Tempest::Device;
  friend class Tempest::DescriptorSet;
  friend class Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

inline Texture2d& textureCast(ZBuffer& a) {
  if(!a.sampleFormat)
    throw std::bad_cast();
  return a.tImpl;
  }

inline const Texture2d& textureCast(const ZBuffer& a) {
  if(!a.sampleFormat)
    throw std::bad_cast();
  return a.tImpl;
  }
}
