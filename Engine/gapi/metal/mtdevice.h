#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {
namespace Detail {

class MtDevice : public AbstractGraphicsApi::Device {
  public:
    MtDevice();
    void waitIdle() override;

  private:

  };

}
}
