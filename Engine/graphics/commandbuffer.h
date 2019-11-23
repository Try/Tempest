#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Encoder>
#include "../utility/dptr.h"

namespace Tempest {

class HeadlessDevice;
class Frame;
class RenderPipeline;
class VideoBuffer;
class Uniforms;
class Texture2d;

template<class T>
class Encoder;

class FrameBufferLayout;
class CommandBuffer;
class PrimaryCommandBuffer;

class CommandBuffer {
  public:
    CommandBuffer()=default;
    CommandBuffer(CommandBuffer&& f)=default;
    virtual ~CommandBuffer();
    CommandBuffer& operator = (CommandBuffer&& other)=default;

    auto startEncoding(Tempest::HeadlessDevice& dev,const FrameBufferLayout &lay) -> Encoder<CommandBuffer>;

  private:
    CommandBuffer(Tempest::HeadlessDevice& dev,AbstractGraphicsApi::CommandBuffer* impl,uint32_t vpWidth,uint32_t vpHeight);

    Tempest::HeadlessDevice*                            dev=nullptr;
    uint32_t                                            vpWidth =0;
    uint32_t                                            vpHeight=0;
    Detail::DPtr<AbstractGraphicsApi::CommandBuffer*>   impl;
    Detail::DSharedPtr<AbstractGraphicsApi::FboLayout*> layout;

  friend class PrimaryCommandBuffer;
  friend class Tempest::HeadlessDevice;
  friend class Tempest::Encoder<CommandBuffer>;
  friend class Tempest::Encoder<PrimaryCommandBuffer>;
  };

class PrimaryCommandBuffer {
  public:
    auto startEncoding(Tempest::HeadlessDevice& dev) -> Encoder<PrimaryCommandBuffer>;

  private:
    PrimaryCommandBuffer(Tempest::HeadlessDevice& dev,AbstractGraphicsApi::CommandBuffer* impl);

    Tempest::HeadlessDevice*                          dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::CommandBuffer*> impl;

  friend class Tempest::HeadlessDevice;
  friend class Tempest::Encoder<PrimaryCommandBuffer>;
  };
}
