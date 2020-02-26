#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxbuffer.h"
#include "dxdevice.h"
#include <cassert>

#include "gapi/graphicsmemutils.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxBuffer::DxBuffer() {
  }

DxBuffer::DxBuffer(ComPtr<ID3D12Resource>&& b, UINT size)
  :impl(std::move(b)), size(size) {
  }

DxBuffer::DxBuffer(Tempest::Detail::DxBuffer&& other)
  :impl(std::move(other.impl)),size(other.size) {
  other.size=0;
  }

void DxBuffer::update(const void* data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  ID3D12Resource& ret = *impl;

  D3D12_RANGE rgn = {off*alignedSz,count*alignedSz};
  void*       mapped=nullptr;
  dxAssert(ret.Map(0,&rgn,&mapped));

  copyUpsample(data,mapped,count,sz,alignedSz);

  ret.Unmap(0,&rgn);
  }

#endif
