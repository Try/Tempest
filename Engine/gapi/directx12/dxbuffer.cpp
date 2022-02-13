#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxbuffer.h"
#include "dxdevice.h"
#include <cassert>

#include "gapi/graphicsmemutils.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxBuffer::DxBuffer(DxDevice* dev, UINT sizeInBytes)
  :dev(dev), sizeInBytes(sizeInBytes) {
  }

DxBuffer::DxBuffer(Tempest::Detail::DxBuffer&& other)
  :dev(other.dev), page(std::move(other.page)), impl(std::move(other.impl)), sizeInBytes(other.sizeInBytes) {
  other.sizeInBytes = 0;
  other.page.page   = nullptr;
  }

DxBuffer::~DxBuffer() {
  if(page.page!=nullptr)
    dev->allocator.free(page);
  }

DxBuffer& DxBuffer::operator=(DxBuffer&& other) {
  std::swap(dev,         other.dev);
  std::swap(page,        other.page);
  std::swap(impl,        other.impl);
  std::swap(sizeInBytes, other.sizeInBytes);
  return *this;
  }

void DxBuffer::update(const void* data, size_t off, size_t count, size_t size, size_t alignedSz) {
  auto& dx = *dev;

  D3D12_HEAP_PROPERTIES prop = {};
  ID3D12Resource&       ret  = *impl;
  ret.GetHeapProperties(&prop,nullptr);

  if(prop.Type==D3D12_HEAP_TYPE_UPLOAD) {
    dx.dataMgr().waitFor(this); // write-after-write case
    updateByMapped(*this,data,off,count,size,alignedSz);
    return;
    }

  auto stage = dx.dataMgr().allocStagingMemory(nullptr,count,size,alignedSz,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::DSharedPtr<DxBuffer*> pstage(new Detail::DxBuffer(std::move(stage)));
  updateByStaging(pstage.handler,data,off,0,count,size,alignedSz);
  }

void DxBuffer::read(void* data, size_t off, size_t size) {
  auto& dx = *dev;

  D3D12_HEAP_PROPERTIES prop = {};
  ID3D12Resource&       ret  = *impl;
  ret.GetHeapProperties(&prop,nullptr);

  if(prop.Type==D3D12_HEAP_TYPE_READBACK) {
    dx.dataMgr().waitFor(this); // write-after-write case
    readFromMapped(*this,data,off,size);
    return;
    }

  auto stage = dx.dataMgr().allocStagingMemory(nullptr,size,1,1,MemUsage::TransferDst,BufferHeap::Readback);
  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->copy(stage,0, *this,off,size);
  cmd->end();
  dx.dataMgr().waitFor(this);
  dx.dataMgr().submitAndWait(std::move(cmd));
  readFromMapped(stage,data,0,size);
  }

void DxBuffer::uploadS3TC(const uint8_t* d, uint32_t w, uint32_t h, uint32_t mipCnt, UINT blockSize) {
  ID3D12Resource& ret = *impl;

  void*       mapped=nullptr;
  dxAssert(ret.Map(0,nullptr,&mapped));

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

  D3D12_RANGE rgn = {0,sizeInBytes};
  ret.Unmap(0,&rgn);
  }

void DxBuffer::updateByStaging(DxBuffer* stage, const void* data, size_t offDst, size_t offSrc,
                               size_t count, size_t size, size_t alignedSz) {
  auto& dx = *dev;

  Detail::DSharedPtr<Buffer*> pbuf(this);
  Detail::DSharedPtr<Buffer*> pstage(stage);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pbuf); // NOTE: DxBuffer may be deleted, before copy is finished
  cmd->hold(pstage);
  cmd->copy(*this, offDst*alignedSz, *stage, offSrc*alignedSz, count*alignedSz);
  cmd->end();

  updateByMapped(*stage,data,offSrc,count,size,alignedSz);
  dx.dataMgr().waitFor(this); // write-after-write case
  dx.dataMgr().submit(std::move(cmd));
  }

void DxBuffer::updateByMapped(DxBuffer& stage, const void* data, size_t off, size_t count, size_t size, size_t alignedSz) {
  D3D12_RANGE rgn    = {off*alignedSz,count*alignedSz};
  void*       mapped = nullptr;
  dxAssert(stage.impl->Map(0,nullptr,&mapped));
  mapped = reinterpret_cast<uint8_t*>(mapped)+off*alignedSz;
  copyUpsample(data,mapped,count,size,alignedSz);
  stage.impl->Unmap(0,&rgn);
  }

void DxBuffer::readFromStaging(DxBuffer& stage, void* data, size_t off, size_t size) {
  auto& dx = *dev;

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->copy(stage,off, *this,off,size);
  cmd->end();
  dx.dataMgr().waitFor(this);
  dx.dataMgr().submitAndWait(std::move(cmd));

  readFromMapped(stage,data,off,size);
  }

void DxBuffer::readFromMapped(DxBuffer& stage, void* data, size_t off, size_t size) {
  D3D12_RANGE rgn = {off,size};
  void*       mapped=nullptr;
  dxAssert(stage.impl->Map(0,&rgn,&mapped));
  mapped = reinterpret_cast<uint8_t*>(mapped)+off;
  std::memcpy(data,mapped,size);
  stage.impl->Unmap(0,nullptr);
  }


DxBufferWithStaging::DxBufferWithStaging(DxBuffer&& base, BufferHeap stagingHeap)
  :DxBuffer(std::move(base)) {
  if(stagingHeap==BufferHeap::Readback) {
    auto  stage = dev->dataMgr().allocStagingMemory(nullptr,sizeInBytes,1,1,MemUsage::TransferDst,BufferHeap::Readback);
    stagingR = Detail::DSharedPtr<DxBuffer*>(new Detail::DxBuffer(std::move(stage)));
    }
  if(stagingHeap==BufferHeap::Upload) {
    auto  stage = dev->dataMgr().allocStagingMemory(nullptr,sizeInBytes,1,1,MemUsage::TransferSrc,BufferHeap::Upload);
    stagingU = Detail::DSharedPtr<DxBuffer*>(new Detail::DxBuffer(std::move(stage)));
    }
  }

void DxBufferWithStaging::update(const void* data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  if(stagingU.handler==nullptr)
    return DxBuffer::update(data,off,count,sz,alignedSz);
  updateByStaging(stagingU.handler,data,off,off,count,sz,alignedSz);
  }

void DxBufferWithStaging::read(void* data, size_t off, size_t size) {
  if(stagingR.handler==nullptr)
    return DxBuffer::read(data,off,size);
  readFromStaging(*stagingR.handler,data,off,size);
  }

#endif
