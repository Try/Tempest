#include "fence.h"
#include <Tempest/Device>

using namespace Tempest;

Fence::Fence(std::shared_ptr<AbstractGraphicsApi::Fence>& f)
  : impl(f) {
  }

Fence::~Fence() {
  if(auto ptr = impl.lock()) {
    ptr->wait();
    }
  }

void Fence::wait() {
  if(auto ptr = impl.lock()) {
    ptr->wait();
    return;
    }
  }

bool Fence::wait(uint64_t time) {
  if(auto ptr = impl.lock()) {
    return ptr->wait(time);
    }
  return true;
  }
