#include "commandbuffer.h"

#include <Tempest/Device>
#include <Tempest/RenderPipeline>
#include <Tempest/Encoder>
#include <Tempest/Except>

using namespace Tempest;

CommandBuffer::CommandBuffer(Device& dev, AbstractGraphicsApi::CommandBundle* impl)
  :dev(&dev),impl(impl) {
  }

CommandBuffer::~CommandBuffer() {
  delete impl.handler;
  }

Encoder<CommandBuffer> CommandBuffer::startEncoding(Device& device, const FrameBufferLayout &lay, Size sz) {
  return startEncoding(device,lay,sz.w,sz.h);
  }

Encoder<CommandBuffer> CommandBuffer::startEncoding(Device& device, const FrameBufferLayout &lay,
                                                    int w, int h) {
  if(impl.handler!=nullptr && impl.handler->isRecording())
    throw ConcurentRecordingException();
  if(impl.handler==nullptr || dev!=&device || layout.handler!=lay.impl.handler) {
    *this = device.commandSecondaryBuffer(lay);
    dev   = &device;
    }
  return Encoder<CommandBuffer>(this,w,h);
  }

PrimaryCommandBuffer::PrimaryCommandBuffer(Device& dev, AbstractGraphicsApi::CommandBuffer *impl)
  :dev(&dev), impl(impl){
  }

PrimaryCommandBuffer::~PrimaryCommandBuffer() {
  delete impl.handler;
  }

Encoder<PrimaryCommandBuffer> PrimaryCommandBuffer::startEncoding(Device& device) {
  if(impl.handler!=nullptr && impl.handler->isRecording())
    throw ConcurentRecordingException();
  if(impl.handler==nullptr || dev!=&device) {
    *this = device.commandBuffer();
    dev   = &device;
    }
  return Encoder<PrimaryCommandBuffer>(this);
  }
