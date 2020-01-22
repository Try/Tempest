#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxbuffer.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxBuffer::DxBuffer() {
  }

DxBuffer::DxBuffer(ComPtr<ID3D12Resource>&& b)
  :impl(std::move(b)){
  }

DxBuffer::DxBuffer(Tempest::Detail::DxBuffer&& other)
  :impl(std::move(other.impl)) {
  }

void DxBuffer::update(const void* data, size_t off, size_t count, size_t sz, size_t alignedSz) {

  }

#endif
