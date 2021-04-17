#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "nsptr.h"

class MTLDevice;

namespace Tempest {
namespace Detail {

class MtDevice : public AbstractGraphicsApi::Device {
  public:
    MtDevice();
    ~MtDevice();
    void waitIdle() override;

    NsPtr impl;
    NsPtr queue;
  };

}
}
