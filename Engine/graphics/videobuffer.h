#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
class Uniforms;

class VideoBuffer {
  public:
    VideoBuffer()=default;
    VideoBuffer(VideoBuffer&&);
    ~VideoBuffer();
    VideoBuffer& operator=(VideoBuffer&&);

  private:
    VideoBuffer(Tempest::Device& dev,AbstractGraphicsApi::Buffer* impl);

    Tempest::Device*                           dev =nullptr;
    Detail::DPtr<AbstractGraphicsApi::Buffer*> impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Uniforms;
  };

}

