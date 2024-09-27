#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Texture2d>

namespace Tempest {

template<class T> T textureCast(ZBuffer& s);
template<class T> T textureCast(const ZBuffer& s);

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

  private:
    ZBuffer(Texture2d&& t, bool nonSampleFormat):tImpl(std::move(t)), nonSampleFormat(nonSampleFormat) {}

    Texture2d tImpl;
    bool      nonSampleFormat=false;

  friend class Tempest::Device;
  friend class Tempest::DescriptorSet;
  friend class Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend T textureCast(ZBuffer& s);

  template<class T>
  friend T textureCast(const ZBuffer& s);
  };

template<>
inline Texture2d& textureCast<Texture2d&>(ZBuffer& a) {
  if(a.nonSampleFormat)
    throw std::bad_cast();
  return a.tImpl;
  }

template<>
inline const Texture2d& textureCast<const Texture2d&>(ZBuffer& a) {
  if(a.nonSampleFormat)
    throw std::bad_cast();
  return a.tImpl;
  }

template<>
inline const Texture2d& textureCast<const Texture2d&>(const ZBuffer& a) {
  if(a.nonSampleFormat)
    throw std::bad_cast();
  return a.tImpl;
  }
}
