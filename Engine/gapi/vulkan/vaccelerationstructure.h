#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {
namespace Detail {

class VDevice;
class VBuffer;

class VAccelerationStructure : public AbstractGraphicsApi::AccelerationStructure {
    public:
      VAccelerationStructure(VDevice& owner, VBuffer& vbo, VBuffer& ibo);
  };

}
}
