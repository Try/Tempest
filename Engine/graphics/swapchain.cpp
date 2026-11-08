#include "swapchain.h"

#include <Tempest/Attachment>
#include <Tempest/Device>

using namespace Tempest;

Swapchain::Swapchain(AbstractGraphicsApi::Swapchain* sw)
  : impl(sw) {
  implReset();
  }

void Swapchain::implReset() {
  imgRingSz = 1;
  imgRing.reset(new Attachment[imgRingSz]);
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
  std::swap(imgRingSz, s.imgRingSz);
  return *this;
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
  auto& ret = imgRing[0];
  ret = Attachment(impl.handler, impl.handler->currentBackBuffer());
  return ret;
  }
