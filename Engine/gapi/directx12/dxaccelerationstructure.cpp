#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxaccelerationstructure.h"

#include "dxdevice.h"
#include "dxbuffer.h"

#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

void DxBlasBuildCtx::pushGeometry(DxDevice& dx,
                                  const DxBuffer& ivbo, size_t vboSz, size_t stride,
                                  const DxBuffer& iibo, size_t iboSz, size_t ioffset, IndexClass icls) {
  auto& vbo = const_cast<DxBuffer&>(ivbo);
  auto& ibo = const_cast<DxBuffer&>(iibo);

  D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
  geometryDesc.Type                                 = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
  geometryDesc.Flags                                = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
  geometryDesc.Triangles.Transform3x4               = 0;
  geometryDesc.Triangles.IndexFormat                = nativeFormat(icls);
  geometryDesc.Triangles.VertexFormat               = DXGI_FORMAT_R32G32B32_FLOAT;
  geometryDesc.Triangles.IndexCount                 = UINT(iboSz);
  geometryDesc.Triangles.VertexCount                = UINT(vboSz);
  geometryDesc.Triangles.IndexBuffer                = ibo.impl->GetGPUVirtualAddress() + ioffset*sizeofIndex(icls);
  geometryDesc.Triangles.VertexBuffer.StartAddress  = vbo.impl->GetGPUVirtualAddress();
  geometryDesc.Triangles.VertexBuffer.StrideInBytes = stride;

  geometry.push_back(geometryDesc);
  }

D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO DxBlasBuildCtx::buildSizes(DxDevice& dx) const {
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  blasInputs     = buildCmd(dx, nullptr);
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO buildSizesInfo = {};

  ComPtr<ID3D12Device5> m_dxrDevice;
  dx.device->QueryInterface(uuid<ID3D12Device5>(), reinterpret_cast<void**>(&m_dxrDevice));
  m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &buildSizesInfo);

  return buildSizesInfo;
  }

D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS DxBlasBuildCtx::buildCmd(DxDevice& dx, DxBuffer* scratch) const {
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
  blasInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  blasInputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  blasInputs.pGeometryDescs = geometry.data();
  blasInputs.NumDescs       = UINT(geometry.size());
  return blasInputs;
  }


DxAccelerationStructure::DxAccelerationStructure(DxDevice& dx, const AbstractGraphicsApi::RtGeometry* geom, size_t size)
  :owner(dx) {
  DxBlasBuildCtx ctx;
  for(size_t i=0; i<size; ++i) {
    auto& vbo     = *reinterpret_cast<const DxBuffer*>(geom[i].vbo);
    auto  vboSz   = geom[i].vboSz;
    auto  stride  = geom[i].stride;
    auto& ibo     = *reinterpret_cast<const DxBuffer*>(geom[i].ibo);
    auto  iboSz   = geom[i].iboSz;
    auto  ioffset = geom[i].ioffset;
    auto  icls    = geom[i].icls;
    ctx.pushGeometry(dx, vbo, vboSz, stride, ibo, iboSz, ioffset, icls);
    }

  const auto buildSizesInfo = ctx.buildSizes(dx);
  if(buildSizesInfo.ResultDataMaxSizeInBytes<=0)
    throw std::system_error(GraphicsErrc::UnsupportedExtension);

  auto  scratch = dx.dataMgr().allocStagingMemory(nullptr, buildSizesInfo.ScratchDataSizeInBytes, MemUsage::ScratchBuffer, BufferHeap::Device);
  impl = dx.allocator.alloc(nullptr, buildSizesInfo.ResultDataMaxSizeInBytes, MemUsage::AsStorage, BufferHeap::Device);

  auto& mgr = dx.dataMgr();
  auto  cmd = dx.dataMgr().get();
  cmd->begin();
  //cmd->hold(scratch);
  cmd->buildBlas(impl.impl.get(), ctx, scratch);
  cmd->end();

  // dx.dataMgr().waitFor(this);
  for(size_t i=0; i<size; ++i) {
    mgr.waitFor(geom[i].vbo);
    mgr.waitFor(geom[i].ibo);
    }

  dx.dataMgr().submitAndWait(std::move(cmd));
  }

DxAccelerationStructure::~DxAccelerationStructure() {
  }


DxTopAccelerationStructure::DxTopAccelerationStructure(DxDevice& dx, const RtInstance* inst, AccelerationStructure*const* as, size_t asSize)
  :owner(dx) {
  ComPtr<ID3D12Device5> m_dxrDevice;
  dx.device->QueryInterface(uuid<ID3D12Device5>(), reinterpret_cast<void**>(&m_dxrDevice));

  Detail::DSharedPtr<DxBuffer*> pBuf;
  if(asSize>0) {
    DxBuffer buf = dx.allocator.alloc(nullptr,asSize*sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
                                      MemUsage::TransferDst | MemUsage::StorageBuffer,BufferHeap::Device);

    pBuf = Detail::DSharedPtr<DxBuffer*>(new Detail::DxBuffer(std::move(buf)));
    }

  for(size_t i=0; i<asSize; ++i) {
    auto blas = reinterpret_cast<DxAccelerationStructure*>(as[i]);

    // NOTE: same as vulkan
    D3D12_RAYTRACING_INSTANCE_DESC objInstance = {};
    for(int x=0; x<3; ++x)
      for(int y=0; y<4; ++y)
        objInstance.Transform[x][y] = inst[i].mat.at(y,x);
    objInstance.InstanceID                          = inst[i].id;
    objInstance.InstanceMask                        = 0xFF;
    objInstance.InstanceContributionToHitGroupIndex = 0;
    objInstance.Flags                               = nativeFormat(inst[i].flags);
    objInstance.AccelerationStructure               = blas->impl.impl->GetGPUVirtualAddress();

    pBuf.handler->update(&objInstance, i*sizeof(objInstance), sizeof(objInstance));
    }

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
  tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  tlasInputs.Flags       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  tlasInputs.NumDescs    = UINT(asSize);
  tlasInputs.Type        = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO buildSizesInfo = {};
  m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &buildSizesInfo);
  if(buildSizesInfo.ResultDataMaxSizeInBytes<=0)
    throw std::system_error(GraphicsErrc::UnsupportedExtension);

  auto  scratch = dx.dataMgr().allocStagingMemory(nullptr,buildSizesInfo.ScratchDataSizeInBytes,MemUsage::ScratchBuffer,BufferHeap::Device);
  impl = dx.allocator.alloc(nullptr, buildSizesInfo.ResultDataMaxSizeInBytes, MemUsage::AsStorage, BufferHeap::Device);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  // cmd->hold(scratch);
  cmd->buildTlas(impl, *pBuf.handler, uint32_t(asSize), scratch);
  cmd->end();

  // dx.dataMgr().waitFor(this);
  dx.dataMgr().submitAndWait(std::move(cmd));
  }

DxTopAccelerationStructure::~DxTopAccelerationStructure() {
  }

#endif
