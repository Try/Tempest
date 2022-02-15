#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class AccelerationStructure final {
  public:
    AccelerationStructure() = default;
    AccelerationStructure(AccelerationStructure&&)=default;
    ~AccelerationStructure()=default;
    AccelerationStructure& operator=(AccelerationStructure&&)=default;

  private:
    AccelerationStructure(Tempest::Device& dev, AbstractGraphicsApi::AccelerationStructure* impl);

    Detail::DSharedPtr<AbstractGraphicsApi::AccelerationStructure*> impl;

  friend class Tempest::Device;
  friend class Encoder<Tempest::CommandBuffer>;
  };

}

