#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtBuffer : public Tempest::AbstractGraphicsApi::Buffer {
  public:
    MtBuffer(id buf);
    ~MtBuffer();

    void  update  (const void* data,size_t off,size_t count,size_t sz,size_t alignedSz) override;
    void  read    (      void* data,size_t off,size_t size) override;

    NsPtr impl;
  };

}
}
