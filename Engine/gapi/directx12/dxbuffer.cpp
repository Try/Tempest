#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxbuffer.h"
#include "dxdevice.h"
#include <cassert>

#include "gapi/graphicsmemutils.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxBuffer::DxBuffer(DxDevice* dev, ComPtr<ID3D12Resource>&& b, UINT sizeInBytes)
  :dev(dev), impl(std::move(b)), sizeInBytes(sizeInBytes) {
  }

DxBuffer::DxBuffer(Tempest::Detail::DxBuffer&& other)
  :dev(other.dev), impl(std::move(other.impl)),sizeInBytes(other.sizeInBytes) {
  other.sizeInBytes=0;
  }

void DxBuffer::update(const void* data, size_t off, size_t count, size_t size, size_t alignedSz) {
  auto& dx = *dev;

  D3D12_HEAP_PROPERTIES prop = {};
  ID3D12Resource&       ret  = *impl;
  ret.GetHeapProperties(&prop,nullptr);

  if(prop.Type==D3D12_HEAP_TYPE_UPLOAD) {
    dx.dataMgr().waitFor(this); // write-after-write case

    D3D12_RANGE rgn    = {off*alignedSz,count*alignedSz};
    void*       mapped = nullptr;
    dxAssert(ret.Map(0,&rgn,&mapped));
    mapped = reinterpret_cast<uint8_t*>(mapped)+off;
    copyUpsample(data,mapped,count,size,alignedSz);
    ret.Unmap(0,&rgn);
    return;
    }

  auto  stage = dx.dataMgr().allocStagingMemory(data,count,size,alignedSz,MemUsage::TransferSrc,BufferHeap::Upload);

  Detail::DSharedPtr<Buffer*> pstage(new Detail::DxBuffer(std::move(stage)));
  Detail::DSharedPtr<Buffer*> pbuf  (this);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pbuf); // NOTE: DxBuffer may be deleted, before copy is finished
  cmd->hold(pstage);
  cmd->copy(*this, off*alignedSz, *pstage.handler,0, count*alignedSz);
  cmd->end();

  dx.dataMgr().waitFor(this); // write-after-write case
  dx.dataMgr().submit(std::move(cmd));
  }

void DxBuffer::read(void* data, size_t off, size_t size) {
  auto& dx = *dev;

  D3D12_HEAP_PROPERTIES prop = {};
  ID3D12Resource&       ret  = *impl;
  ret.GetHeapProperties(&prop,nullptr);

  if(prop.Type==D3D12_HEAP_TYPE_READBACK) {
    dx.dataMgr().waitFor(this); // write-after-write case

    D3D12_RANGE rgn = {off,size};
    void*       mapped=nullptr;
    dxAssert(ret.Map(0,&rgn,&mapped));
    mapped = reinterpret_cast<uint8_t*>(mapped)+off;
    std::memcpy(data,mapped,size);
    ret.Unmap(0,nullptr);
    return;
    }

  auto  stage = dx.dataMgr().allocStagingMemory(nullptr,size,1,1,MemUsage::TransferDst,BufferHeap::Readback);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->copy(stage,0, *this,off,size);
  cmd->end();
  dx.dataMgr().waitFor(this);
  dx.dataMgr().submitAndWait(std::move(cmd));

  D3D12_RANGE rgn = {0,size};
  void*       mapped=nullptr;
  dxAssert(stage.impl->Map(0,&rgn,&mapped));
  mapped = reinterpret_cast<uint8_t*>(mapped);
  std::memcpy(data,mapped,size);
  stage.impl->Unmap(0,nullptr);
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
