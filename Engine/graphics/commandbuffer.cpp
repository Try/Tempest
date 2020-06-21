#include "commandbuffer.h"

#include <Tempest/Device>
#include <Tempest/RenderPipeline>
#include <Tempest/Encoder>
#include <Tempest/Except>

using namespace Tempest;

CommandBuffer::CommandBuffer(Device& dev, AbstractGraphicsApi::CommandBuffer* impl)
  :dev(&dev),impl(impl) {
  }

CommandBuffer::~CommandBuffer() {
  delete impl.handler;
  }

Encoder<CommandBuffer> CommandBuffer::startEncoding(Device& device) {
  if(impl.handler!=nullptr && impl.handler->isRecording())
    throw ConcurentRecordingException();
  if(impl.handler==nullptr || dev!=&device) {
    *this  = device.commandBuffer();
    dev    = &device;
    }
  return Encoder<CommandBuffer>(this);
  }
