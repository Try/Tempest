#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/Metal.hpp>

#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;
class MtBuffer;

class MtAccelerationStructure : public Tempest::AbstractGraphicsApi::AccelerationStructure {
  public:
    MtAccelerationStructure(MtDevice& owner,
                            MtBuffer& vbo, size_t vboSz, size_t stride,
                            MtBuffer& ibo, size_t iboSz, size_t ioffset, Detail::IndexClass icls);
    ~MtAccelerationStructure();

    MtDevice&                         owner;
    NsPtr<MTL::AccelerationStructure> impl;
  };

class MtTopAccelerationStructure : public AbstractGraphicsApi::AccelerationStructure {
  public:
    MtTopAccelerationStructure(MtDevice& owner, const RtInstance* inst, AccelerationStructure* const * as, size_t asSize);
    ~MtTopAccelerationStructure();

    MtDevice&                         owner;
    NsPtr<MTL::AccelerationStructure> impl;
    NsPtr<MTL::Buffer>                instances;
  };
}
}
