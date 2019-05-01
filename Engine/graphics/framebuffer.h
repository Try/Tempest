#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;

class FrameBuffer final {
  public:
    FrameBuffer()=default;
    FrameBuffer(FrameBuffer&& f)=default;
    ~FrameBuffer();
    FrameBuffer& operator = (FrameBuffer&& other)=default;

    uint32_t w() const { return mw; }
    uint32_t h() const { return mh; }

  private:
    FrameBuffer(Tempest::Device& dev,AbstractGraphicsApi::Fbo* f,uint32_t w,uint32_t h);

    Tempest::Device*                        dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Fbo*> impl;
    uint32_t                                mw=0, mh=0;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  };
}
