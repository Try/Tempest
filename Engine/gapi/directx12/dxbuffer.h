#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include "comptr.h"

namespace Tempest {
namespace Detail {

class DxBuffer : public AbstractGraphicsApi::Buffer {
  public:
    DxBuffer() = delete;
    DxBuffer(ComPtr<ID3D12Resource>&& b, UINT sizeInBytes);
    DxBuffer(DxBuffer&& other);

    void  update(const void* data,size_t off,size_t count,size_t sz,size_t alignedSz) override;

    ComPtr<ID3D12Resource> impl;
    UINT                   sizeInBytes=0;
  };

}
}

