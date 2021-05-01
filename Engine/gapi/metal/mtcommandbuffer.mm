#include "mtcommandbuffer.h"

#include "mtbuffer.h"
#include "mtdevice.h"
#include "mtframebuffer.h"
#include "mtpipeline.h"
#include "mtrenderpass.h"
#include "mtpipelinelay.h"
#include "mtdescriptorarray.h"

#include <Metal/MTLRenderCommandEncoder.h>
#include <Metal/MTLCommandQueue.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtCommandBuffer::MtCommandBuffer(MtDevice& dev)
  : device(dev) {
  reset();
  }

bool MtCommandBuffer::isRecording() const {
  return enc!=nil;
  }

void MtCommandBuffer::begin() {
  reset();
  }

void MtCommandBuffer::end() {
  if(enc==nil)
    return;
  [enc endEncoding];
  enc = nil;
  }

void MtCommandBuffer::reset() {
  MTLCommandBufferDescriptor* desc = [[MTLCommandBufferDescriptor alloc] init];
  desc.retainedReferences = NO;

  impl = [device.queue commandBufferWithDescriptor:desc];

  [desc release];
  }

void MtCommandBuffer::beginRenderPass(AbstractGraphicsApi::Fbo *f,
                                      AbstractGraphicsApi::Pass *p,
                                      uint32_t width, uint32_t height) {
  auto& fbo  = *reinterpret_cast<MtFramebuffer*>(f);
  auto& pass = *reinterpret_cast<MtRenderPass*> (p);

  MTLRenderPassDescriptor* desc = fbo.instance(pass);
  enc = [impl renderCommandEncoderWithDescriptor:desc];
  curFbo = &fbo;

  [enc setFrontFacingWinding:MTLWindingCounterClockwise];
  setViewport(Rect(0,0,width,height));
  }

void MtCommandBuffer::endRenderPass() {
  [enc endEncoding];
  enc    = nil;
  curFbo = nullptr;
  }

void MtCommandBuffer::beginCompute() {
  enc = [impl computeCommandEncoder];
  }

void MtCommandBuffer::endCompute() {
  [enc endEncoding];
  enc = nil;
  }

void MtCommandBuffer::changeLayout(AbstractGraphicsApi::Buffer &buf, BufferLayout prev, BufferLayout next) {
  }

void MtCommandBuffer::changeLayout(AbstractGraphicsApi::Attach &img, TextureLayout prev, TextureLayout next, bool byRegion) {
  }

void MtCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture &image, TextureLayout defLayout, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  }

void MtCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline &p) {
  auto& px   = reinterpret_cast<MtPipeline&>(p);
  auto& inst = px.inst(*curFbo->layout.handler);

  if(curFbo->depth!=nullptr)
    [enc setDepthStencilState:px.depthStZ]; else
    [enc setDepthStencilState:px.depthStNoZ];
  [enc setRenderPipelineState:inst.pso];
  [enc setCullMode:px.cullMode];
  }

void MtCommandBuffer::setComputePipeline(AbstractGraphicsApi::CompPipeline &p) {
  auto& px = reinterpret_cast<MtCompPipeline&>(p);
  [enc setComputePipelineState:px.impl];
  }

void MtCommandBuffer::setBytes(AbstractGraphicsApi::Pipeline &p, const void *data, size_t size) {

  }

void MtCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline &p,
                                  AbstractGraphicsApi::Desc &u) {
  }

void MtCommandBuffer::setBytes(AbstractGraphicsApi::CompPipeline &p,
                               const void *data, size_t size) {


  }

void MtCommandBuffer::setUniforms(AbstractGraphicsApi::CompPipeline &p,
                                  AbstractGraphicsApi::Desc &u) {
  auto& d   = reinterpret_cast<MtDescriptorArray&>(u);
  auto& lay = d.lay.handler->lay;
  for(size_t i=0; i<lay.size(); ++i) {
    auto& l = lay[i];
    switch(l.cls) {
      case ShaderReflection::Push:
        break;
      case ShaderReflection::Ubo:
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW:
        [enc setBuffer:d.desc[i].val
                offset:0
               atIndex:i];
        break;
      case ShaderReflection::Texture:
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW:
        [enc setTexture:d.desc[i].val];
        break;
      }
    }
  }

void MtCommandBuffer::setViewport(const Rect &r) {
  MTLViewport v = {};
  v.originX = r.x;
  v.originY = r.y;
  v.width   = r.w;
  v.height  = r.h;
  v.znear   = 0;
  v.zfar    = 1;

  [enc setViewport:v];
  }

void MtCommandBuffer::setVbo(const AbstractGraphicsApi::Buffer &b) {
  auto& bx = reinterpret_cast<const MtBuffer&>(b);
  [enc setVertexBuffer:bx.impl
                offset:0
               atIndex:0];
  }

void MtCommandBuffer::setIbo(const AbstractGraphicsApi::Buffer &b, IndexClass cls) {
  curIbo = &reinterpret_cast<const MtBuffer&>(b);
  switch(cls) {
    case IndexClass::i16:
      iboType = MTLIndexTypeUInt16;
      break;
    case IndexClass::i32:
      iboType = MTLIndexTypeUInt32;
      break;
    }
  }

void MtCommandBuffer::draw(size_t offset, size_t vertexCount, size_t firstInstance, size_t instanceCount) {
  [enc drawPrimitives:MTLPrimitiveTypeTriangle
                      vertexStart:offset
                      vertexCount:vertexCount
                      instanceCount:instanceCount
                      baseInstance:firstInstance];
  }

void MtCommandBuffer::drawIndexed(size_t ioffset, size_t isize, size_t voffset, size_t firstInstance, size_t instanceCount) {
  [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                             indexCount:isize
                             indexType:iboType
                             indexBuffer:curIbo->impl
                             indexBufferOffset:ioffset
                             instanceCount:instanceCount
                             baseVertex:voffset
                             baseInstance:firstInstance
                             ];
  }

void MtCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  MTLSize sz;
  sz.width  = x;
  sz.height = y;
  sz.depth  = z;

  MTLSize th;
  th.width  = 1;
  th.height = 1;
  th.depth  = 1;

  [enc dispatchThreadgroups:sz threadsPerThreadgroup:th];
  }
