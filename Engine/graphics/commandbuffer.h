#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Encoder>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
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

class CommandBuffer final {
  public:
    CommandBuffer()=default;
    CommandBuffer(CommandBuffer&& f)=default;
    ~CommandBuffer();
    CommandBuffer& operator = (CommandBuffer&& other)=default;

    auto startEncoding(Tempest::Device& dev,const FrameBufferLayout &lay) -> Encoder<CommandBuffer>;

  private:
    CommandBuffer(Tempest::Device& dev,AbstractGraphicsApi::CommandBuffer* impl,uint32_t vpWidth,uint32_t vpHeight);

    Tempest::Device*                                    dev=nullptr;
    uint32_t                                            vpWidth =0;
    uint32_t                                            vpHeight=0;
    Detail::DPtr<AbstractGraphicsApi::CommandBuffer*>   impl;
    Detail::DSharedPtr<AbstractGraphicsApi::FboLayout*> layout;

  friend class PrimaryCommandBuffer;
  friend class Tempest::Device;
  friend class Tempest::Encoder<CommandBuffer>;
  friend class Tempest::Encoder<PrimaryCommandBuffer>;
  };

class PrimaryCommandBuffer final {
  public:
    PrimaryCommandBuffer()=default;
    PrimaryCommandBuffer(PrimaryCommandBuffer&& f)=default;
    PrimaryCommandBuffer& operator = (PrimaryCommandBuffer&& other)=default;
    ~PrimaryCommandBuffer();

    auto startEncoding(Tempest::Device& dev) -> Encoder<PrimaryCommandBuffer>;

  private:
    PrimaryCommandBuffer(Tempest::Device& dev,AbstractGraphicsApi::CommandBuffer* impl);

    Tempest::Device*                                    dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::CommandBuffer*>   impl;

  friend class Tempest::Device;
  friend class Tempest::Encoder<PrimaryCommandBuffer>;
  };
}
