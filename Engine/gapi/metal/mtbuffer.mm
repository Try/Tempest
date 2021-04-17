#include "mtbuffer.h"

#import <Metal/MTLBuffer.h>

using namespace Tempest::Detail;

MtBuffer::MtBuffer(id buf)
  :impl((__bridge void*)(buf)){
  }

MtBuffer::~MtBuffer() {
  }

void MtBuffer::update(const void *data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  // TODO
  }

void MtBuffer::read(void *data, size_t off, size_t size) {
  // TODO
  }
