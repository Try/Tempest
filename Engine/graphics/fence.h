#pragma once

#include <Tempest/AbstractGraphicsApi>

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
    Fence(std::shared_ptr<AbstractGraphicsApi::Fence>& f);

    std::shared_ptr<AbstractGraphicsApi::Fence> impl;

  friend class Tempest::Device;
  };
}
