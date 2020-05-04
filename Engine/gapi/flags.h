#pragma once

#include <cstdint>

namespace Tempest {

enum class MemUsage : uint8_t {
  TransferSrc =1<<0,
  TransferDst =1<<1,
  UniformBit  =1<<2,
  VertexBuffer=1<<3,
  IndexBuffer =1<<4,
  };

inline MemUsage operator | (MemUsage a,const MemUsage& b) {
  return MemUsage(uint8_t(a)|uint8_t(b));
  }

inline MemUsage operator & (MemUsage a,const MemUsage& b) {
  return MemUsage(uint8_t(a)&uint8_t(b));
  }


enum class BufferFlags : uint8_t {
  Staging = 1<<0,
  Static  = 1<<1,
  Dynamic = 1<<2
  };

inline BufferFlags operator | (BufferFlags a,const BufferFlags& b) {
  return BufferFlags(uint8_t(a)|uint8_t(b));
  }

inline BufferFlags operator & (BufferFlags a,const BufferFlags& b) {
  return BufferFlags(uint8_t(a)&uint8_t(b));
  }


enum class TextureLayout : uint8_t {
  Undefined,
  Sampler,
  ColorAttach,
  DepthAttach,
  Present,
  TransferSrc,
  TransferDest
  };
}
