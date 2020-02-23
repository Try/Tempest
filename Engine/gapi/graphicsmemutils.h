#pragma once

#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace Tempest {
namespace Detail {

static inline void copyUpsample(const void *src, void* dest,
                                size_t count, size_t size, size_t alignedSz){
  if(src==nullptr)
    return;

  if(size==alignedSz) {
    std::memcpy(dest,src,size*count);
    } else {
    auto s = reinterpret_cast<const uint8_t*>(src);
    auto d = reinterpret_cast<uint8_t*>(dest);
    for(size_t i=0;i<count;++i) {
      std::memcpy(d+i*alignedSz,s+i*size,size);
      }
    }
  }

}
}
