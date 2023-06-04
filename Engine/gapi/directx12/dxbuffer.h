#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include "comptr.h"
#include "dxallocator.h"

namespace Tempest {
namespace Detail {

class DxDevice;

class DxBuffer : public AbstractGraphicsApi::Buffer {
  public:
    DxBuffer() = default;
    DxBuffer(DxDevice* dev, UINT sizeInBytes, UINT appSize);
    DxBuffer(DxBuffer&& other);
    ~DxBuffer();

    DxBuffer& operator=(DxBuffer&& other);

    void  update(const void* data,size_t off,size_t count,size_t sz,size_t alignedSz) override;
    void  read  (void* data, size_t off, size_t sz) override;

    void  uploadS3TC(const uint8_t* d, uint32_t w, uint32_t h, uint32_t mip, UINT blockSize);

    DxDevice*               dev = nullptr;
    DxAllocator::Allocation page={};

    ComPtr<ID3D12Resource>  impl;
    NonUniqResId            nonUniqId   = NonUniqResId::I_None;
    UINT                    sizeInBytes = 0;
    UINT                    appSize     = 0;

  protected:
    void  updateByStaging(DxBuffer* stage, const void* data, size_t offDst, size_t offSrc, size_t count, size_t sz, size_t alignedSz);
    void  updateByMapped (DxBuffer& stage, const void* data,size_t off,size_t count,size_t sz,size_t alignedSz);

    void  readFromStaging(DxBuffer& stage, void* data, size_t off, size_t size);
    void  readFromMapped (DxBuffer& stage, void* data, size_t off, size_t size);
  };

class DxBufferWithStaging : public DxBuffer {
  public:
    DxBufferWithStaging(DxBuffer&& base, BufferHeap stagingHeap);

    void  update(const void* data,size_t off,size_t count,size_t sz,size_t alignedSz) override;
    void  read  (void* data, size_t off, size_t sz) override;

  private:
    Detail::DSharedPtr<DxBuffer*> stagingU = {};
    Detail::DSharedPtr<DxBuffer*> stagingR = {};
  };

}
}

