#include "mtbuffer.h"
#include "mtdevice.h"

#include "utility/compiller_hints.h"
#include "gapi/graphicsmemutils.h"

#import <Metal/MTLDevice.h>
#import <Metal/MTLCommandQueue.h>
#import <Metal/MTLCommandBuffer.h>

using namespace Tempest::Detail;

MtBuffer::MtBuffer(const MtDevice& dev, id<MTLBuffer> buf, MTLResourceOptions flg)
  :dev(dev), impl(buf), flg(flg) {
  }

MtBuffer::~MtBuffer() {
  [impl release];
  }

void MtBuffer::update(const void *data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  if(T_LIKELY((flg&MTLResourceStorageModePrivate)==0)) {
    implUpdate(data,off,count,sz,alignedSz);
    return;
    }

  MTLResourceOptions opt   = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
  id<MTLDevice>      dx    = dev.impl;
  id<MTLBuffer>      stage = [dx newBufferWithLength:count*alignedSz options:opt];
  copyUpsample(data, stage.contents, count, sz, alignedSz);

  id<MTLCommandBuffer>      cmd   = [dev.queue commandBuffer];
  id<MTLBlitCommandEncoder> enc   = [cmd blitCommandEncoder];
  [enc copyFromBuffer:stage
                      sourceOffset:0
                      toBuffer:impl
                      destinationOffset:0 size:count*alignedSz];
  [enc endEncoding];
  [cmd commit];

  // TODO: implement proper upload engine
  [cmd waitUntilCompleted];

  [enc release];
  [cmd release];
  }

void MtBuffer::read(void *data, size_t off, size_t size) {
  MTLResourceOptions opt   = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
  id<MTLDevice>      dx    = dev.impl;
  id<MTLBuffer>      stage = [dx newBufferWithLength:size options:opt];

  id<MTLCommandBuffer>      cmd   = [dev.queue commandBuffer];
  id<MTLBlitCommandEncoder> enc   = [cmd blitCommandEncoder];
  [enc copyFromBuffer:impl
                      sourceOffset:off
                      toBuffer:stage
                      destinationOffset:0
                      size:size];
  [enc endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];

  [enc release];
  [cmd release];

  std::memcpy(data,stage.contents,size);
  }

void MtBuffer::implUpdate(const void *data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  id<MTLBuffer> buf = impl;
  auto ptr = reinterpret_cast<uint8_t*>([buf contents]);

  copyUpsample(data, ptr+off, count, sz, alignedSz);
  if((flg&MTLResourceStorageModeManaged)==0)
    return;

  NSRange rgn;
  rgn.location = off;
  rgn.length   = count*alignedSz;
  [buf didModifyRange: rgn];
  }
