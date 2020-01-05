#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
class Uniforms;
template<class T>
class Encoder;

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
    VideoBuffer(Tempest::Device& dev, AbstractGraphicsApi::PBuffer &&impl, size_t size);

    Detail::DSharedPtr<AbstractGraphicsApi::Buffer*> impl;
    size_t                                           sz=0;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Uniforms;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

}

