#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/MTLBuffer.h>
#include <Metal/MTLAccelerationStructure.h>

namespace Tempest {
namespace Detail {

class MtDevice;
class MtBuffer;

class MtAccelerationStructure : public Tempest::AbstractGraphicsApi::AccelerationStructure {
  public:
    MtAccelerationStructure(MtDevice& owner,
                            MtBuffer& vbo, size_t vboSz, size_t voffset, size_t stride,
                            MtBuffer& ibo, size_t iboSz, Detail::IndexClass icls);
    ~MtAccelerationStructure();

    MtDevice&                    owner;
    id<MTLAccelerationStructure> impl;
  };

class MtTopAccelerationStructure : public AbstractGraphicsApi::AccelerationStructure {
  public:
    MtTopAccelerationStructure(MtDevice& owner, const RtInstance* inst, AccelerationStructure* const * as, size_t asSize);
    ~MtTopAccelerationStructure();

    MtDevice&                    owner;
    id<MTLAccelerationStructure> impl;
    id<MTLBuffer>                instances;
  };
}
}
