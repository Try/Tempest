#pragma once

#include <cstdint>

namespace Tempest {

enum class MemUsage : uint16_t {
  TransferSrc  =1<<0,
  TransferDst  =1<<1,
  UniformBuffer=1<<2,
  VertexBuffer =1<<3,
  IndexBuffer  =1<<4,
  StorageBuffer=1<<5,
  ScratchBuffer=1<<6,
  AsStorage    =1<<7,
  Indirect     =1<<8,
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

  Index            = 1 << 8,
  Vertex           = 1 << 9,
  Uniform          = 1 << 10,

  UavReadComp      = 1 << 11,
  UavWriteComp     = 1 << 12,

  UavReadGr        = 1 << 13,
  UavWriteGr       = 1 << 14,

  RtAsRead         = 1 << 15,
  RtAsWrite        = 1 << 16,

  TransferSrcDst   = (TransferSrc    | TransferDst),
  UavReadWriteComp = (UavReadComp    | UavWriteComp),
  UavReadWriteGr   = (UavReadGr      | UavWriteGr  ),
  UavReadWriteAll  = (UavReadWriteGr | UavReadWriteComp),

  UavRead = (TransferSrc | Index | Vertex | Uniform | UavReadComp | UavReadGr | RtAsRead),
  AnyRead = (UavRead | Sampler | DepthReadOnly),
  };

inline ResourceAccess operator | (ResourceAccess a,const ResourceAccess& b) {
  return ResourceAccess(uint32_t(a)|uint32_t(b));
  }

inline ResourceAccess operator & (ResourceAccess a,const ResourceAccess& b) {
  return ResourceAccess(uint32_t(a)&uint32_t(b));
  }

enum NonUniqResId : uint8_t {
  I_None = 0x0,
  I_Img  = 0x1,
  I_Buf  = 0x2,
  I_Ssbo = 0x4,
  T_RtAs = 0x8,
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
  S_Transfer,
  S_RtAs,
  S_Compute,
  S_Graphics,

  S_First = S_Transfer,
  S_Count = S_Graphics+1,
  };

}
