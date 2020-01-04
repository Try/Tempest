#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class HeadlessDevice;
class Device;
class Swapchain;

class Semaphore final {
  public:
    Semaphore(Semaphore&& f)=default;
    ~Semaphore();
    Semaphore& operator = (Semaphore&& other)=default;

  private:
    Semaphore(Tempest::HeadlessDevice& dev,AbstractGraphicsApi::Semaphore* f);

    Tempest::HeadlessDevice*                      dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Semaphore*> impl;

  friend class Tempest::Device;
  friend class Tempest::HeadlessDevice;
  friend class Tempest::Swapchain;
  };

}
