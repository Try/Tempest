#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxbuffer.h"
#include "dxdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxBuffer::DxBuffer(DxDevice* dev, UINT sizeInBytes, UINT appSize)
  :dev(dev), sizeInBytes(sizeInBytes), appSize(appSize) {
  }

DxBuffer::DxBuffer(DxBuffer&& other)
  :dev(other.dev), page(std::move(other.page)), impl(std::move(other.impl)), nonUniqId(other.nonUniqId),
    sizeInBytes(other.sizeInBytes), appSize(other.appSize) {
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
  std::swap(nonUniqId,   other.nonUniqId);
  std::swap(sizeInBytes, other.sizeInBytes);
  std::swap(appSize,     other.appSize);
  return *this;
  }

void DxBuffer::update(const void* data, size_t off, size_t size) {
  auto& dx = *dev;

  D3D12_HEAP_PROPERTIES prop = {};
  ID3D12Resource&       ret  = *impl;
  ret.GetHeapProperties(&prop,nullptr);

  if(prop.Type==D3D12_HEAP_TYPE_UPLOAD || prop.Type==D3D12_HEAP_TYPE_CUSTOM) {
    dx.dataMgr().waitFor(this); // write-after-write case
    updateByMapped(*this,data,off,size);
    return;
    }

  auto stage = dx.dataMgr().allocStagingMemory(nullptr,size,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::DSharedPtr<DxBuffer*> pstage(new Detail::DxBuffer(std::move(stage)));
  updateByStaging(pstage.handler,data,off,0,size);
  }

void DxBuffer::fill(uint32_t data, size_t off, size_t size) {
  auto& dx = *dev;

  D3D12_HEAP_PROPERTIES prop = {};
  ID3D12Resource&       ret  = *impl;
  ret.GetHeapProperties(&prop,nullptr);

  if(prop.Type==D3D12_HEAP_TYPE_UPLOAD || prop.Type==D3D12_HEAP_TYPE_CUSTOM) {
    dx.dataMgr().waitFor(this); // write-after-write case
    fillByMapped(*this,data,off,size);
    return;
    }

  auto stage = dx.dataMgr().allocStagingMemory(nullptr,size,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::DSharedPtr<DxBuffer*> pstage(new Detail::DxBuffer(std::move(stage)));
  fillByStaging(pstage.handler,data,off,0,size);
  }

void DxBuffer::read(void* data, size_t off, size_t size) {
  auto& dx = *dev;

  D3D12_HEAP_PROPERTIES prop = {};
  ID3D12Resource&       ret  = *impl;
  ret.GetHeapProperties(&prop,nullptr);

  if(prop.Type==D3D12_HEAP_TYPE_READBACK || prop.Type==D3D12_HEAP_TYPE_CUSTOM) {
    dx.dataMgr().waitFor(this); // write-after-write case
    readFromMapped(*this,data,off,size);
    return;
    }

  auto stage = dx.dataMgr().allocStagingMemory(nullptr,size,MemUsage::TransferDst,BufferHeap::Readback);
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

void DxBuffer::updateByStaging(DxBuffer* stage, const void* data, size_t offDst, size_t offSrc, size_t size) {
  auto& dx = *dev;

  Detail::DSharedPtr<Buffer*> pbuf(this);
  Detail::DSharedPtr<Buffer*> pstage(stage);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pbuf); // NOTE: DxBuffer may be deleted, before copy is finished
  cmd->hold(pstage);
  cmd->copy(*this, offDst, *stage, offSrc, size);
  cmd->end();

  updateByMapped(*stage,data,offSrc,size);
  dx.dataMgr().waitFor(this); // write-after-write case
  dx.dataMgr().submit(std::move(cmd));
  }

void DxBuffer::updateByMapped(DxBuffer& stage, const void* data, size_t off, size_t size) {
  D3D12_RANGE rgn    = {off,size};
  void*       mapped = nullptr;
  dxAssert(stage.impl->Map(0,nullptr,&mapped));
  mapped = reinterpret_cast<uint8_t*>(mapped)+off;
  std::memcpy(mapped, data, size);
  stage.impl->Unmap(0,&rgn);
  }

void DxBuffer::fillByStaging(DxBuffer* stage, uint32_t data, size_t offDst, size_t offSrc, size_t size) {
  auto& dx = *dev;

  Detail::DSharedPtr<Buffer*> pbuf(this);
  Detail::DSharedPtr<Buffer*> pstage(stage);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pbuf); // NOTE: DxBuffer may be deleted, before copy is finished
  cmd->hold(pstage);
  cmd->copy(*this, offDst, *stage, offSrc, size);
  cmd->end();

  fillByMapped(*stage,data,offSrc,size);
  dx.dataMgr().waitFor(this); // write-after-write case
  dx.dataMgr().submit(std::move(cmd));
  }

void DxBuffer::fillByMapped(DxBuffer& stage, uint32_t data, size_t off, size_t size) {
  D3D12_RANGE rgn    = {off,size};
  void*       mapped = nullptr;
  dxAssert(stage.impl->Map(0,nullptr,&mapped));
  mapped = reinterpret_cast<uint8_t*>(mapped)+off;
  std::fill_n(reinterpret_cast<uint32_t*>(mapped), size/sizeof(uint32_t), data);
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

#endif
