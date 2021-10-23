#include "swapchain.h"

#include <Tempest/Attachment>
#include <Tempest/Device>

using namespace Tempest;

Swapchain::Swapchain(AbstractGraphicsApi::Swapchain* sw)
  : impl(sw) {
  implReset();
  }

void Swapchain::implReset() {
  size_t cnt = imageCount();
  img.reset(new Attachment[cnt]);
  for(uint32_t i=0;i<cnt;++i)
    img[i] = Attachment(impl.handler,i);
  }

Swapchain::Swapchain(Device& dev, SystemApi::Window* w) {
  *this = dev.swapchain(w);
  }

Swapchain::~Swapchain() {
  delete impl.handler;
  }

Swapchain& Swapchain::operator = (Swapchain&& s) {
  std::swap(impl, s.impl);
  std::swap(img,  s.img);
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

uint32_t Swapchain::imageCount() const {
  return impl.handler->imageCount();
  }

Attachment& Swapchain::operator[](size_t id) {
  return img[id];
  }

const Attachment& Swapchain::operator[](size_t id) const {
  return img[id];
  }

uint32_t Swapchain::currentImage() const {
  return impl.handler->currentBackBufferIndex();
  }
