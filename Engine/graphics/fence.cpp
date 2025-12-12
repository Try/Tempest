#include "fence.h"

using namespace Tempest;

Fence::Fence(std::shared_ptr<AbstractGraphicsApi::Fence>& f)
  : impl(f) {
  }

Fence::~Fence() {
  if(impl != nullptr)  {
    impl->wait();
    }
  }

void Fence::wait() {
  if(impl != nullptr) {
    impl->wait();
    return;
    }
  }

bool Fence::wait(uint64_t time) {
  if(impl != nullptr) {
    return impl->wait(time);
    }
  return true;
  }
