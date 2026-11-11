#include "swapchain.h"

#include <Tempest/Attachment>
#include <Tempest/Device>

using namespace Tempest;

Swapchain::Swapchain(AbstractGraphicsApi::Swapchain* sw)
  : impl(sw) {
  implReset();
  }

Swapchain::Swapchain(Device& dev, SystemApi::Window* w) {
  *this = dev.swapchain(w);
  }

Swapchain::~Swapchain() {
  delete impl.handler;
  }

Swapchain& Swapchain::operator = (Swapchain&& s) {
  std::swap(impl,      s.impl);
  std::swap(imgRing,   s.imgRing);
  std::swap(imgRingAt, s.imgRingAt);
  return *this;
  }

void Swapchain::implReset() {
  imgRingAt = 0;
  imgRing.reset(new Attachment[RingSize]);
  }

uint32_t Swapchain::w() const {
  return impl.handler->w();
  }

uint32_t Swapchain::h() const {
  return impl.handler->h();
  }

void Swapchain::reset() {
  impl.handler->reset();
  implReset();
  }

Attachment& Swapchain::acquire() {
  auto& ret = imgRing[imgRingAt];
  ret = Attachment(impl.handler, impl.handler->currentBackBuffer());
  return ret;
  }
