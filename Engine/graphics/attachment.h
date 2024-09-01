#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Swapchain>
#include <Tempest/Texture2d>

namespace Tempest {

template<class T> T textureCast(Attachment& s);
template<class T> T textureCast(const Attachment& s);

class Attachment final {
  public:
    Attachment()=default;
    Attachment(Attachment&&)=default;
    ~Attachment()=default;
    Attachment& operator=(Attachment&&)=default;

    int              w() const;
    int              h() const;
    Size             size() const;
    bool             isEmpty() const;

  private:
    Attachment(Texture2d&& t):tImpl(std::move(t)){}
    Attachment(AbstractGraphicsApi::Swapchain* sw, uint32_t id);

    struct SwImage {
      AbstractGraphicsApi::Swapchain* swapchain = nullptr;
      uint32_t                        id        = 0;
      };
    Texture2d tImpl;
    SwImage   sImpl;

  friend class Tempest::Device;
  friend class Tempest::Swapchain;
  friend class Encoder<Tempest::CommandBuffer>;

  template<class T> friend T textureCast(Attachment& a);
  template<class T> friend T textureCast(const Attachment& a);
  };

template<>
inline Texture2d& textureCast<Texture2d&>(Attachment& a) {
  if(a.sImpl.swapchain)
    throw std::bad_cast();
  return a.tImpl;
  }

template<>
inline const Texture2d& textureCast<const Texture2d&>(Attachment& a) {
  if(a.sImpl.swapchain)
    throw std::bad_cast();
  return a.tImpl;
  }

template<>
inline const Texture2d& textureCast<const Texture2d&>(const Attachment& a) {
  if(a.sImpl.swapchain)
    throw std::bad_cast();
  return a.tImpl;
  }
}
