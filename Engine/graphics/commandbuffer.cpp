#include "commandbuffer.h"

#include <Tempest/Device>
#include <Tempest/RenderPipeline>
#include <Tempest/Encoder>
#include <Tempest/Except>

using namespace Tempest;

CommandBuffer::CommandBuffer(HeadlessDevice &dev, AbstractGraphicsApi::CommandBuffer *impl, uint32_t vpWidth, uint32_t vpHeight)
  :dev(&dev),impl(impl) {
  this->vpWidth  = vpWidth;
  this->vpHeight = vpHeight;
  }

CommandBuffer::~CommandBuffer() {
  delete impl.handler;
  }

Encoder<CommandBuffer> CommandBuffer::startEncoding(HeadlessDevice &device, const FrameBufferLayout &lay) {
  if(impl.handler!=nullptr && impl.handler->isRecording())
    throw ConcurentRecordingException();
  if(impl.handler==nullptr || dev!=&device || layout.handler!=lay.impl.handler) {
    *this = device.commandSecondaryBuffer(lay);
    dev   = &device;
    }
  return Encoder<CommandBuffer>(this);
  }

PrimaryCommandBuffer::PrimaryCommandBuffer(HeadlessDevice &dev, AbstractGraphicsApi::CommandBuffer *impl)
  :dev(&dev), impl(impl){
  }

PrimaryCommandBuffer::~PrimaryCommandBuffer() {
  delete impl.handler;
  }

Encoder<PrimaryCommandBuffer> PrimaryCommandBuffer::startEncoding(HeadlessDevice &device) {
  if(impl.handler!=nullptr && impl.handler->isRecording())
    throw ConcurentRecordingException();
  if(impl.handler==nullptr || dev!=&device) {
    *this = device.commandBuffer();
    dev   = &device;
    }
  return Encoder<PrimaryCommandBuffer>(this);
  }
