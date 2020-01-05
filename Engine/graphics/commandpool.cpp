#include "commandpool.h"

#include <Tempest/Device>

using namespace Tempest;

CommandPool::CommandPool(Tempest::Device &dev, AbstractGraphicsApi::CmdPool *impl)
  :dev(&dev),impl(impl) {
  }

CommandPool::~CommandPool() {
  delete impl.handler;
  }
