#pragma once

#include <cstdint>

namespace Tempest {

enum class MemUsage : uint16_t {
  Initialized  =1<<0,
  TransferSrc  =1<<1,
  TransferDst  =1<<2,
  UniformBuffer=1<<3,
  VertexBuffer =1<<4,
  IndexBuffer  =1<<5,
  StorageBuffer=1<<6,
  ScratchBuffer=1<<7,
  AsStorage    =1<<8,
  Indirect     =1<<9,
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
  TransferSrc      = 1 << 0,
  TransferDst      = 1 << 1,
  TransferHost     = 1 << 2,

  Present          = 1 << 3,

  Sampler          = 1 << 4,
  ColorAttach      = 1 << 5,
  DepthAttach      = 1 << 6,
  DepthReadOnly    = 1 << 7,

  Indirect         = 1 << 8,
  Index            = 1 << 9,
  Vertex           = 1 << 10,
  Uniform          = 1 << 11,

  UavReadComp      = 1 << 12,
  UavWriteComp     = 1 << 13,

  UavReadGr        = 1 << 14,
  UavWriteGr       = 1 << 15,

  RtAsRead         = 1 << 16,
  RtAsWrite        = 1 << 17,

  TransferSrcDst   = (TransferSrc    | TransferDst),
  UavReadWriteComp = (UavReadComp    | UavWriteComp),
  UavReadWriteGr   = (UavReadGr      | UavWriteGr  ),
  UavReadWriteAll  = (UavReadWriteGr | UavReadWriteComp),

  UavRead = (TransferSrc | Index | Vertex | Uniform | UavReadComp | UavReadGr | RtAsRead),
  AnyRead = (Indirect | UavRead | Sampler | DepthReadOnly),
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
  S_RtAs,
  S_Compute,
  S_Graphics,
  S_Indirect,

  S_First = S_Transfer,
  S_Count = S_Indirect+1,
  };

}
