#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Texture2d>

namespace Tempest {

class Device;
class CommandBuffer;
class PrimaryCommandBuffer;
class Texture2d;
class Swapchain;

//! attachment 2d texture class
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

    friend       Texture2d& textureCast(Attachment& a);
    friend const Texture2d& textureCast(const Attachment& a);

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
  friend class Tempest::Uniforms;
  friend class Encoder<Tempest::CommandBuffer>;
  friend class Encoder<Tempest::PrimaryCommandBuffer>;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

inline Texture2d& textureCast(Attachment& a) {
  if(a.sImpl.swapchain)
    throw std::bad_cast();
  return a.tImpl;
  }

inline const Texture2d& textureCast(const Attachment& a) {
  if(a.sImpl.swapchain)
    throw std::bad_cast();
  return a.tImpl;
  }
}
