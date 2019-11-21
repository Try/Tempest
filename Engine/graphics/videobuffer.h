#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class HeadlessDevice;
class CommandBuffer;
class Uniforms;

class VideoBuffer {
  public:
    VideoBuffer()=default;
    VideoBuffer(VideoBuffer&&);
    ~VideoBuffer();
    VideoBuffer& operator=(VideoBuffer&&);

    void   setAsMapped();
    void   update(const void* data,size_t offset,size_t size);
    size_t size() const { return sz; }

  private:
    VideoBuffer(Tempest::HeadlessDevice& dev, AbstractGraphicsApi::PBuffer &&impl, size_t size);

    Detail::DSharedPtr<AbstractGraphicsApi::Buffer*> impl;
    size_t                                           sz=0;

  friend class Tempest::HeadlessDevice;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Uniforms;
  };

}

