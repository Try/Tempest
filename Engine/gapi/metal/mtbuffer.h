#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/Metal.hpp>
#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;

class MtBuffer : public Tempest::AbstractGraphicsApi::Buffer {
  public:
    MtBuffer(MtDevice& dev, const void* data, size_t count, size_t sz, size_t alignedSz, MTL::ResourceOptions flg);
    ~MtBuffer();

    void  update  (const void* data, size_t off, size_t count, size_t sz, size_t alignedSz) override;
    void  read    (      void* data, size_t off, size_t size) override;

    MtDevice&          dev;
    NsPtr<MTL::Buffer> impl;

  private:
    void implUpdate(const void *data, size_t off, size_t count, size_t sz, size_t alignedSz);
  };

}
}
