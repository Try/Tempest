#include "fence.h"
#include <Tempest/Device>

using namespace Tempest;

Fence::Fence(AbstractGraphicsApi::Fence *impl)
  :impl(impl) {
  }

Fence::Fence(std::shared_ptr<AbstractGraphicsApi::Fence>& f)
  : impl2(f) {
  }

Fence::~Fence() {
  if(auto ptr = impl2.lock()) {
    ptr->wait();
    }
  delete impl.handler;
  }

void Fence::wait() {
  if(auto ptr = impl2.lock()) {
    ptr->wait();
    return;
    }
  if(impl.handler!=nullptr)
    impl.handler->wait();
  }

bool Fence::wait(uint64_t time) {
  if(auto ptr = impl2.lock()) {
    return ptr->wait(time);
    }
  if(impl.handler!=nullptr)
    return impl.handler->wait(time);
  return true;
  }
