#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;

class FrameBuffer {
  public:
    FrameBuffer(FrameBuffer&& f)=default;
    ~FrameBuffer();
    FrameBuffer& operator = (FrameBuffer&& other)=default;

  private:
    FrameBuffer(Tempest::Device& dev,AbstractGraphicsApi::Fbo* f);

    Tempest::Device*                        dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Fbo*> impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  };
}
