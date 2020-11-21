#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxbuffer.h"
#include "dxdevice.h"
#include <cassert>

#include "gapi/graphicsmemutils.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxBuffer::DxBuffer(ComPtr<ID3D12Resource>&& b, UINT sizeInBytes)
  :impl(std::move(b)), sizeInBytes(sizeInBytes) {
  }

DxBuffer::DxBuffer(Tempest::Detail::DxBuffer&& other)
  :impl(std::move(other.impl)),sizeInBytes(other.sizeInBytes) {
  other.sizeInBytes=0;
  }

void DxBuffer::update(const void* data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  ID3D12Resource& ret = *impl;

  D3D12_RANGE rgn    = {off*alignedSz,count*alignedSz};
  void*       mapped = nullptr;
  dxAssert(ret.Map(0,&rgn,&mapped));
  mapped = reinterpret_cast<uint8_t*>(mapped)+off;

  copyUpsample(data,mapped,count,sz,alignedSz);

  ret.Unmap(0,&rgn);
  }

void DxBuffer::read(void* data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  ID3D12Resource& ret = *impl;

  D3D12_RANGE rgn = {off,sz};
  void*       mapped=nullptr;
  dxAssert(ret.Map(0,&rgn,&mapped));
  mapped = reinterpret_cast<uint8_t*>(mapped)+off;

  copyUpsample(mapped,data,count,sz,alignedSz);

  ret.Unmap(0,nullptr);
  }

void DxBuffer::read(void* data, size_t off, size_t sz) {
  ID3D12Resource& ret = *impl;

  D3D12_RANGE rgn = {off,sz};
  void*       mapped=nullptr;
  dxAssert(ret.Map(0,&rgn,&mapped));
  mapped = reinterpret_cast<uint8_t*>(mapped)+off;

  std::memcpy(data,mapped,sz);

  ret.Unmap(0,nullptr);
  }

void DxBuffer::uploadS3TC(const uint8_t* d, uint32_t w, uint32_t h, uint32_t mipCnt, UINT blockSize) {
  ID3D12Resource& ret = *impl;

  D3D12_RANGE rgn = {0,sizeInBytes};
  void*       mapped=nullptr;
  dxAssert(ret.Map(0,&rgn,&mapped));

  uint8_t* b = reinterpret_cast<uint8_t*>(mapped);

  uint32_t bufferSize = 0, stageSize = 0;
  for(uint32_t i=0; i<mipCnt; i++) {
    UINT wBlk = (w+3)/4;
    UINT hBlk = (h+3)/4;

    UINT pitchS = wBlk*blockSize;
    UINT pitchA = alignTo(pitchS,D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    for(uint32_t r=0;r<hBlk;++r) {
      std::memcpy(b+stageSize,d+bufferSize,pitchS);
      bufferSize += pitchS;
      stageSize  += pitchA;
      }

    stageSize = alignTo(stageSize,D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    w = std::max<uint32_t>(1,w/2);
    h = std::max<uint32_t>(1,h/2);
    }

  ret.Unmap(0,nullptr);
  }

#endif
