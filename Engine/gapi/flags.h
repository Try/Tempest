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
  Readback = 3,
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
  ComputeRead      = 1 << 10,
  ComputeWrite     = 1 << 11,
  ComputeReadWrite = (ComputeRead | ComputeWrite),
  };

inline ResourceAccess operator | (ResourceAccess a,const ResourceAccess& b) {
  return ResourceAccess(uint32_t(a)|uint32_t(b));
  }

inline ResourceAccess operator & (ResourceAccess a,const ResourceAccess& b) {
  return ResourceAccess(uint32_t(a)&uint32_t(b));
  }

}
