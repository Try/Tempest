#include "mtcommandbuffer.h"

#include "mtdevice.h"
#include "mtframebuffer.h"
#include "mtrenderpass.h"

#include <Metal/MTLCommandBuffer.h>
#include <Metal/MTLRenderCommandEncoder.h>
#include <Metal/MTLCommandQueue.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtCommandBuffer::MtCommandBuffer(MtDevice& dev)
  : device(dev) {
  reset();
  }

bool MtCommandBuffer::isRecording() const {
  return enc.ptr!=nullptr;
  }

void MtCommandBuffer::begin() {}

void MtCommandBuffer::end() {
  if(enc.ptr==nullptr)
    return;
  [enc.get() endEncoding];
  enc = NsPtr();
  }

void MtCommandBuffer::reset() {
  MTLCommandBufferDescriptor* desc = [[MTLCommandBufferDescriptor alloc] init];
  desc.retainedReferences = NO;

  id<MTLCommandQueue> queue = device.queue.get();
  impl = NsPtr((__bridge void*)([queue commandBufferWithDescriptor:desc]));

  [desc release];
  }

void MtCommandBuffer::beginRenderPass(AbstractGraphicsApi::Fbo *f,
                                      AbstractGraphicsApi::Pass *p,
                                      uint32_t /*width*/, uint32_t /*height*/) {
  auto& fbo  = *reinterpret_cast<MtFramebuffer*>(f);
  auto& pass = *reinterpret_cast<MtRenderPass*> (p);

  MTLRenderPassDescriptor* desc = fbo.instance(pass);
  enc = NsPtr((__bridge void*)([impl.get() renderCommandEncoderWithDescriptor: desc]));
  }

void MtCommandBuffer::endRenderPass() {
  [enc.get() endEncoding];
  enc = NsPtr();
  }

void MtCommandBuffer::beginCompute() {
  }

void MtCommandBuffer::endCompute() {
  }

void MtCommandBuffer::changeLayout(AbstractGraphicsApi::Buffer &buf, BufferLayout prev, BufferLayout next) {
  }

void MtCommandBuffer::changeLayout(AbstractGraphicsApi::Attach &img, TextureLayout prev, TextureLayout next, bool byRegion) {
  }

void MtCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture &image, TextureLayout defLayout, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  }

void MtCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline &p, uint32_t w, uint32_t h) {

  }

void Detail::MtCommandBuffer::setComputePipeline(AbstractGraphicsApi::CompPipeline &p) {

  }

void MtCommandBuffer::setBytes(AbstractGraphicsApi::Pipeline &p, const void *data, size_t size) {

  }

void MtCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u) {

  }

void MtCommandBuffer::setBytes(AbstractGraphicsApi::CompPipeline &p, const void *data, size_t size) {

  }

void MtCommandBuffer::setUniforms(AbstractGraphicsApi::CompPipeline &p, AbstractGraphicsApi::Desc &u) {

  }

void MtCommandBuffer::setViewport(const Rect &r) {

  }

void MtCommandBuffer::setVbo(const AbstractGraphicsApi::Buffer &b) {

  }

void MtCommandBuffer::setIbo(const AbstractGraphicsApi::Buffer &b, IndexClass cls) {

  }

void MtCommandBuffer::draw(size_t offset, size_t vertexCount, size_t firstInstance, size_t instanceCount) {

  }

void MtCommandBuffer::drawIndexed(size_t ioffset, size_t isize, size_t voffset, size_t firstInstance, size_t instanceCount) {

  }

void MtCommandBuffer::dispatch(size_t x, size_t y, size_t z) {

  }
