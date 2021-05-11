#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <Metal/MTLSampler.h>
#include <Metal/MTLDevice.h>

#include "utility/spinlock.h"

namespace Tempest {
namespace Detail {

class MtDevice;

class MtSamplerCache {
  public:
    MtSamplerCache(id<MTLDevice> dev);
    ~MtSamplerCache();

    id<MTLSamplerState> get(Sampler2d src);

  private:
    id<MTLSamplerState> mkSampler(const Sampler2d& src);

    struct Entry {
      Sampler2d           src;
      id<MTLSamplerState> val;
      };

    id<MTLDevice>       dev;
    id<MTLSamplerState> def;

    SpinLock            sync;
    std::vector<Entry>  values;
  };

}
}
