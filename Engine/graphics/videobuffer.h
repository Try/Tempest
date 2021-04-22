#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
class DescriptorSet;
template<class T>
class Encoder;

class VideoBuffer {
  public:
    VideoBuffer()=default;
    VideoBuffer(VideoBuffer&&);
    ~VideoBuffer();
    VideoBuffer& operator=(VideoBuffer&&);

    void   update(const void* data, size_t offset, size_t count, size_t size, size_t alignedSz);
    size_t size() const { return sz; }

  private:
    VideoBuffer(AbstractGraphicsApi::PBuffer &&impl, size_t size);

    Detail::DSharedPtr<AbstractGraphicsApi::Buffer*> impl;
    size_t                                           sz=0;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::DescriptorSet;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

}

