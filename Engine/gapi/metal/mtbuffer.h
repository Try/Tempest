#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "nsptr.h"

#include <Metal/MTLBuffer.h>

namespace Tempest {
namespace Detail {

class MtDevice;

class MtBuffer : public Tempest::AbstractGraphicsApi::Buffer {
  public:
    MtBuffer(const MtDevice& dev, id buf, MTLResourceOptions flg);
    ~MtBuffer();

    void  update  (const void* data, size_t off, size_t count, size_t sz, size_t alignedSz) override;
    void  read    (      void* data, size_t off, size_t size) override;

    const MtDevice&          dev;
    NsPtr                    impl;
    const MTLResourceOptions flg = 0;

  private:
    void implUpdate(const void *data, size_t off, size_t count, size_t sz, size_t alignedSz);
  };

}
}
