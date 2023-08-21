#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>
#include "dxbuffer.h"

namespace Tempest {
namespace Detail {

class DxDevice;

struct DxBlasBuildCtx : AbstractGraphicsApi::BlasBuildCtx {
  void pushGeometry(DxDevice& dx,
                    const DxBuffer& vbo, size_t vboSz, size_t stride,
                    const DxBuffer& ibo, size_t iboSz, size_t ioffset, IndexClass icls);

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO buildSizes(DxDevice& dx) const;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  buildCmd (DxDevice& dx, DxBuffer* scratch) const;

  std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry;
  };

class DxAccelerationStructure : public AbstractGraphicsApi::AccelerationStructure {
  public:
    DxAccelerationStructure(DxDevice& owner, const AbstractGraphicsApi::RtGeometry* geom, size_t size);
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
