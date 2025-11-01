#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;

class Fence final {
  public:
    Fence() = default;
    Fence(Fence&& f)=default;
    ~Fence();
    Fence& operator = (Fence&& other)=default;

    void wait();
    bool wait(uint64_t time);

  private:
    Fence(AbstractGraphicsApi::Fence* f);

    Detail::DPtr<AbstractGraphicsApi::Fence*> impl;

  friend class Tempest::Device;
  };
}
