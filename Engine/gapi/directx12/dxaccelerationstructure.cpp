#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxaccelerationstructure.h"

#include "dxdevice.h"
#include "dxbuffer.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxAccelerationStructure::DxAccelerationStructure(DxDevice& dx,
                                                 DxBuffer& vbo, size_t vboSz, size_t offset, size_t stride,
                                                 DxBuffer& ibo, size_t iboSz, IndexClass icls)
  :owner(dx) {
  ComPtr<ID3D12Device5> m_dxrDevice;
  dx.device->QueryInterface(__uuidof(ID3D12Device5), reinterpret_cast<void**>(&m_dxrDevice));

  D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
  geometryDesc.Type                                 = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
  geometryDesc.Flags                                = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
  geometryDesc.Triangles.Transform3x4               = 0;
  geometryDesc.Triangles.IndexFormat                = nativeFormat(icls);
  geometryDesc.Triangles.VertexFormat               = DXGI_FORMAT_R32G32B32_FLOAT;
  geometryDesc.Triangles.IndexCount                 = UINT(iboSz);
  geometryDesc.Triangles.VertexCount                = UINT(vboSz);
  geometryDesc.Triangles.IndexBuffer                = 0;
  geometryDesc.Triangles.VertexBuffer.StartAddress  = 0;
  geometryDesc.Triangles.VertexBuffer.StrideInBytes = stride;

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  blasInputs = {};
  blasInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  blasInputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  blasInputs.pGeometryDescs = &geometryDesc;
  blasInputs.NumDescs       = 1;

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO buildSizesInfo = {};
  m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &buildSizesInfo);
  if(buildSizesInfo.ResultDataMaxSizeInBytes<=0)
    throw std::system_error(GraphicsErrc::UnsupportedExtension);

  auto  scratch = dx.dataMgr().allocStagingMemory(nullptr,buildSizesInfo.ScratchDataSizeInBytes,1,1,
                                                  MemUsage::ScratchBuffer,BufferHeap::Device);
  impl = dx.allocator.alloc(nullptr, buildSizesInfo.ResultDataMaxSizeInBytes,1,1, MemUsage::AsStorage,BufferHeap::Device);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  //cmd->hold(scratch);
  cmd->buildBlas(impl.impl.get(), vbo,vboSz,offset,stride, ibo,iboSz,icls, scratch);
  cmd->end();

  // dx.dataMgr().waitFor(this);
  dx.dataMgr().submitAndWait(std::move(cmd));
  }

DxAccelerationStructure::~DxAccelerationStructure() {
  }


DxTopAccelerationStructure::DxTopAccelerationStructure(DxDevice& dx, DxAccelerationStructure* obj)
  :owner(dx) {
  ComPtr<ID3D12Device5> m_dxrDevice;
  dx.device->QueryInterface(__uuidof(ID3D12Device5), reinterpret_cast<void**>(&m_dxrDevice));

  // NOTE: same as vulkan
  D3D12_RAYTRACING_INSTANCE_DESC objInstance = {};
  objInstance.Transform[0][0]                     = 1.0;
  objInstance.Transform[1][1]                     = 1.0;
  objInstance.Transform[2][2]                     = 1.0;
  objInstance.InstanceID                          = 0;
  objInstance.InstanceMask                        = 0xFF;
  objInstance.InstanceContributionToHitGroupIndex = 0;
  objInstance.Flags                               = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
  objInstance.AccelerationStructure               = obj->impl.impl->GetGPUVirtualAddress();

  Detail::DSharedPtr<DxBuffer*> pBuf;
  {
  DxBuffer buf = dx.allocator.alloc(nullptr,1,sizeof(objInstance),sizeof(objInstance),
                                   MemUsage::TransferDst | MemUsage::StorageBuffer,BufferHeap::Device);

  pBuf = Detail::DSharedPtr<DxBuffer*>(new Detail::DxBuffer(std::move(buf)));
  pBuf.handler->update(&objInstance,0,1, sizeof(objInstance), sizeof(objInstance));
  }

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
  tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  tlasInputs.Flags       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  tlasInputs.NumDescs    = 1;
  tlasInputs.Type        = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO buildSizesInfo = {};
  m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &buildSizesInfo);
  if(buildSizesInfo.ResultDataMaxSizeInBytes<=0)
    throw std::system_error(GraphicsErrc::UnsupportedExtension);

  auto  scratch = dx.dataMgr().allocStagingMemory(nullptr,buildSizesInfo.ScratchDataSizeInBytes,1,1,
                                                  MemUsage::ScratchBuffer,BufferHeap::Device);
  impl = dx.allocator.alloc(nullptr, buildSizesInfo.ResultDataMaxSizeInBytes,1,1, MemUsage::AsStorage,BufferHeap::Device);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  // cmd->hold(scratch);
  cmd->buildTlas(impl, *pBuf.handler, 1, scratch);
  cmd->end();

  // dx.dataMgr().waitFor(this);
  dx.dataMgr().submitAndWait(std::move(cmd));
  }

DxTopAccelerationStructure::~DxTopAccelerationStructure() {
  }

#endif
