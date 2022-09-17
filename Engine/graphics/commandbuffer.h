#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Encoder>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class Frame;
class RenderPipeline;
class DescriptorSet;
class Texture2d;

template<class T>
class Encoder;

class FrameBufferLayout;
class CommandBuffer;

class CommandBuffer final {
  public:
    CommandBuffer()=default;
    CommandBuffer(CommandBuffer&& f)=default;
    ~CommandBuffer();
    CommandBuffer& operator = (CommandBuffer&& other)=default;

    auto startEncoding(Tempest::Device& dev) -> Encoder<CommandBuffer>;

  private:
    CommandBuffer(Tempest::Device& dev, AbstractGraphicsApi::CommandBuffer* impl);

    Tempest::Device*                                    dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::CommandBuffer*>   impl;

  friend class Tempest::Device;
  friend class Tempest::Encoder<CommandBuffer>;
  };

}
