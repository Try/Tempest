#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class HeadlessDevice;
class CommandBuffer;
class PrimaryCommandBuffer;
class Frame;
class FrameBuffer;
class Texture2d;

class FrameBufferLayout final {
  public:
    uint32_t w() const { return mw; }
    uint32_t h() const { return mh; }

    bool operator == (const FrameBufferLayout& fbo) const;
    bool operator != (const FrameBufferLayout& fbo) const;

  private:
    FrameBufferLayout()=default;
    FrameBufferLayout(Detail::DSharedPtr<AbstractGraphicsApi::FboLayout*>&& f,uint32_t w,uint32_t h);

    Detail::DSharedPtr<AbstractGraphicsApi::FboLayout*> impl;
    uint32_t                                            mw=0, mh=0;

  friend class Tempest::Device;
  friend class Tempest::HeadlessDevice;
  friend class Tempest::FrameBuffer;
  friend class Tempest::CommandBuffer;
  };

class FrameBuffer final {
  public:
    FrameBuffer()=default;
    FrameBuffer(FrameBuffer&& f)=default;
    ~FrameBuffer();
    FrameBuffer& operator = (FrameBuffer&& other)=default;

    uint32_t w() const { return lay.mw; }
    uint32_t h() const { return lay.mh; }

    auto     layout() const -> const FrameBufferLayout&;

  private:
    FrameBuffer(Tempest::HeadlessDevice& dev,
                Detail::DSharedPtr<AbstractGraphicsApi::Fbo*>&& f,
                FrameBufferLayout&& lay);

    Tempest::HeadlessDevice*                      dev=nullptr;
    Detail::DSharedPtr<AbstractGraphicsApi::Fbo*> impl;
    FrameBufferLayout                             lay;

  friend class Tempest::Device;
  friend class Tempest::HeadlessDevice;
  friend class Tempest::Encoder<Tempest::PrimaryCommandBuffer>;
  };
}
