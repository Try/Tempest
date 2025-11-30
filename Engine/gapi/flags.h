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
enum class ResourceLayout : uint32_t {
  None          = 0,

  Default       = 1,
  TransferSrc   = 2,
  TransferDst   = 3,

  ColorAttach   = 4,
  DepthAttach   = 5,
  DepthReadOnly = 6,

  Indirect      = 7,
  };

enum class SyncStage : uint32_t {
  None          = 0,

  TransferSrc   = 1 << 0,
  TransferDst   = 1 << 1,
  TransferHost  = 1 << 2,
  Indirect      = 1 << 3,

  GraphicsRead  = 1 << 4,
  GraphicsWrite = 1 << 5,
  GraphicsDraw  = 1 << 6,
  GraphicsDepth = 1 << 7,

  ComputeRead   = 1 << 8,
  ComputeWrite  = 1 << 9,
  RtAsRead      = 1 << 10,
  RtAsWrite     = 1 << 11,

  Transfer      = (TransferSrc  | TransferDst),
  Graphics      = (GraphicsRead | GraphicsWrite | GraphicsDraw | GraphicsDepth),

  AllRead       = (TransferSrc | Indirect | ComputeRead | GraphicsRead | RtAsRead),
  };

inline SyncStage operator | (SyncStage a, const SyncStage& b) {
  return SyncStage(uint32_t(a)|uint32_t(b));
  }

inline SyncStage operator & (SyncStage a, const SyncStage& b) {
  return SyncStage(uint32_t(a)&uint32_t(b));
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
