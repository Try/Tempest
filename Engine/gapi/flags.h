#pragma once

#include <cstdint>

namespace Tempest {

enum class MemUsage : uint16_t {
  Initialized   = 1<<0,
  Transfer      = 1<<1, // implied by default
  UniformBuffer = 1<<2,
  VertexBuffer  = 1<<3,
  IndexBuffer   = 1<<4,
  StorageBuffer = 1<<5,
  ScratchBuffer = 1<<6,
  AsStorage     = 1<<7,
  Indirect      = 1<<8,
  };

inline MemUsage operator | (MemUsage a,const MemUsage& b) {
  return MemUsage(uint16_t(a)|uint16_t(b));
  }

inline MemUsage operator & (MemUsage a,const MemUsage& b) {
  return MemUsage(uint16_t(a)&uint16_t(b));
  }


enum class BufferHeap : uint8_t {
  Device   = 0,
  Upload   = 1,
  Readback = 2,
  };

// TODO: move away from public header
enum class ResourceAccess : uint32_t {
  None             = 0,
  // layouts
  Default          = 1 << 0,
  TransferSrc      = 1 << 1,
  TransferDst      = 1 << 2,
  Present          = 1 << 3,

  TransferHost     = 1 << 4,
  Sampler          = 1 << 5,
  ColorAttach      = 1 << 6,
  DepthAttach      = 1 << 7,
  DepthReadOnly    = 1 << 8,

  // stages
  Indirect         = 1 << 9,
  Index            = 1 << 10,
  Vertex           = 1 << 11,
  Uniform          = 1 << 12,

  UavReadComp      = 1 << 13,
  UavWriteComp     = 1 << 14,

  UavReadGr        = 1 << 15,
  UavWriteGr       = 1 << 16,

  RtAsRead         = 1 << 17,
  RtAsWrite        = 1 << 18,

  TransferSrcDst   = (TransferSrc    | TransferDst),
  UavReadWriteComp = (UavReadComp    | UavWriteComp),
  UavReadWriteGr   = (UavReadGr      | UavWriteGr  ),
  UavReadWriteAll  = (UavReadWriteGr | UavReadWriteComp),

  UavRead = (TransferSrc | Index | Vertex | Uniform | UavReadComp | UavReadGr | RtAsRead),
  // AnyRead = (Indirect | UavRead | Sampler | DepthReadOnly),
  };

inline ResourceAccess operator | (ResourceAccess a,const ResourceAccess& b) {
  return ResourceAccess(uint32_t(a)|uint32_t(b));
  }

inline ResourceAccess operator & (ResourceAccess a,const ResourceAccess& b) {
  return ResourceAccess(uint32_t(a)&uint32_t(b));
  }

enum NonUniqResId : uint32_t {
  I_None = 0x0,
  };

inline NonUniqResId operator | (NonUniqResId a,const NonUniqResId& b) {
  return NonUniqResId(uint32_t(a)|uint32_t(b));
  }

inline void operator |= (NonUniqResId& a,const NonUniqResId& b) {
  a = NonUniqResId(uint32_t(a)|uint32_t(b));
  }

inline NonUniqResId operator & (NonUniqResId a,const NonUniqResId& b) {
  return NonUniqResId(uint32_t(a)&uint32_t(b));
  }

enum PipelineStage : uint8_t {
  S_Transfer,
  S_Indirect,
  S_RtAs,
  S_Compute,
  S_Graphics,

  S_First = S_Transfer,
  S_Count = S_Graphics+1,
  };

}
