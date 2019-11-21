#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class HeadlessDevice;

class CommandPool {
  public:
    CommandPool()=default;
    CommandPool(CommandPool&&)=default;
    ~CommandPool();
    CommandPool& operator=(CommandPool&&)=default;

  private:
    CommandPool(Tempest::HeadlessDevice& dev,AbstractGraphicsApi::CmdPool* impl);

    Tempest::HeadlessDevice*                    dev =nullptr;
    Detail::DPtr<AbstractGraphicsApi::CmdPool*> impl;

  friend class Tempest::HeadlessDevice;
  };

}
