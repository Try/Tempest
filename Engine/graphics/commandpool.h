#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;

class CommandPool {
  public:
    CommandPool()=default;
    CommandPool(CommandPool&&)=default;
    ~CommandPool();
    CommandPool& operator=(CommandPool&&)=default;

  private:
    CommandPool(Tempest::Device& dev,AbstractGraphicsApi::CmdPool* impl);

    Tempest::Device*                           dev =nullptr;
    Detail::DPtr<AbstractGraphicsApi::CmdPool*> impl;

  friend class Tempest::Device;
  };

}
