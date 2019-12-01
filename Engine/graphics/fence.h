#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class HeadlessDevice;

class Fence final {
  public:
    Fence(Fence&& f)=default;
    ~Fence();
    Fence& operator = (Fence&& other)=default;

    void wait();
    void reset();

  private:
    Fence(Tempest::HeadlessDevice& dev,AbstractGraphicsApi::Fence* f);

    Tempest::HeadlessDevice*                  dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Fence*> impl;

  friend class Tempest::HeadlessDevice;
  };
}
