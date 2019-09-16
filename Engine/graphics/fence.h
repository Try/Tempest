#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;

class Fence final {
  public:
    Fence(Tempest::Device& owner);
    Fence(Fence&& f)=default;
    ~Fence();
    Fence& operator = (Fence&& other)=default;

    void wait();
    void reset();

  private:
    Fence(Tempest::Device& dev,AbstractGraphicsApi::Fence* f);

    Tempest::Device*                          dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Fence*> impl;

  friend class Tempest::Device;
  };
}
