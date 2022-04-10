#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"
#include "dxbuffer.h"

namespace Tempest {
namespace Detail {

class DxDevice;

class DxAccelerationStructure : public AbstractGraphicsApi::AccelerationStructure {
  public:
    DxAccelerationStructure(DxDevice& owner,
                            DxBuffer& vbo, size_t vboSz, size_t offset, size_t stride,
                            DxBuffer& ibo, size_t iboSz, Detail::IndexClass icls);
    ~DxAccelerationStructure();

    DxDevice& owner;
    DxBuffer  impl;
  };

class DxTopAccelerationStructure : public AbstractGraphicsApi::AccelerationStructure {
  public:
    DxTopAccelerationStructure(DxDevice& owner, const RtInstance* inst, AccelerationStructure* const * as, size_t asSize);
    ~DxTopAccelerationStructure();

    DxDevice& owner;
    DxBuffer  impl;
  };

}
}
