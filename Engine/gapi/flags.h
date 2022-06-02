#pragma once

#include <cstdint>

namespace Tempest {

enum class MemUsage : uint8_t {
  TransferSrc  =1<<0,
  TransferDst  =1<<1,
  UniformBuffer=1<<2,
  VertexBuffer =1<<3,
  IndexBuffer  =1<<4,
  StorageBuffer=1<<5,
  ScratchBuffer=1<<6,
  AsStorage    =1<<7,
  };

inline MemUsage operator | (MemUsage a,const MemUsage& b) {
  return MemUsage(uint8_t(a)|uint8_t(b));
  }

inline MemUsage operator & (MemUsage a,const MemUsage& b) {
  return MemUsage(uint8_t(a)&uint8_t(b));
  }


enum class BufferHeap : uint8_t {
  Device   = 0,
  Upload   = 1,
  Readback = 2,
  };

// TODO: move away from public header
enum ResourceAccess : uint32_t {
  None             = 0,
  TransferSrc      = 1 << 0,
  TransferDst      = 1 << 1,

  Present          = 1 << 2,

  Sampler          = 1 << 3,
  ColorAttach      = 1 << 4,
  DepthAttach      = 1 << 5,
  Unordered        = 1 << 6,

  Index            = 1 << 7,
  Vertex           = 1 << 8,
  Uniform          = 1 << 9,

  UavReadComp      = 1 << 10,
  UavWriteComp     = 1 << 11,

  UavReadGr        = 1 << 12,
  UavWriteGr       = 1 << 13,

  // for debug view
  UavReadWriteComp = (UavReadComp    | UavWriteComp),
  UavReadWriteGr   = (UavReadGr      | UavWriteGr  ),
  UavReadWriteAll  = (UavReadWriteGr | UavReadWriteComp),
  };

inline ResourceAccess operator | (ResourceAccess a,const ResourceAccess& b) {
  return ResourceAccess(uint32_t(a)|uint32_t(b));
  }

inline ResourceAccess operator & (ResourceAccess a,const ResourceAccess& b) {
  return ResourceAccess(uint32_t(a)&uint32_t(b));
  }

enum NonUniqResId : uint8_t {
  I_None = 0x0,
  I_Buf  = 0x1,
  I_Ssbo = 0x2,
  I_Img  = 0x4,
  };

inline NonUniqResId operator | (NonUniqResId a,const NonUniqResId& b) {
  return NonUniqResId(uint8_t(a)|uint8_t(b));
  }

inline void operator |= (NonUniqResId& a,const NonUniqResId& b) {
  a = NonUniqResId(uint8_t(a)|uint8_t(b));
  }

inline NonUniqResId operator & (NonUniqResId a,const NonUniqResId& b) {
  return NonUniqResId(uint8_t(a)&uint8_t(b));
  }

enum PipelineStage : uint8_t {
  S_Compute,
  S_Graphics,

  S_First = S_Compute,
  S_Count = S_Graphics+1,
  };

}
