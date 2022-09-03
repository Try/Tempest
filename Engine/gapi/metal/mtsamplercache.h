#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/Metal.hpp>
#include <mutex>

#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;

class MtSamplerCache {
  public:
    MtSamplerCache(MTL::Device& dev);
    ~MtSamplerCache();

    MTL::SamplerState& get(Sampler src);

  private:
    NsPtr<MTL::SamplerState> mkSampler(const Sampler& src);

    struct Entry {
      Sampler                  src;
      NsPtr<MTL::SamplerState> val;
      };

    MTL::Device&             dev;
    NsPtr<MTL::SamplerState> def;

    std::mutex               sync;
    std::vector<Entry>       values;
  };

}
}
