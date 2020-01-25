#include "swapchain.h"

#include <Tempest/Attachment>
#include <Tempest/Device>
#include <Tempest/Semaphore>

using namespace Tempest;

Swapchain::Swapchain(AbstractGraphicsApi::Device& dev, AbstractGraphicsApi& api, SystemApi::Window* w, uint8_t maxFramesInFlight)
  : api(&api), dev(&dev), hwnd(w), implMaxFramesInFlight(maxFramesInFlight) {
  reset();
  }

Swapchain::Swapchain(Device& dev, SystemApi::Window* w) {
  *this = dev.swapchain(w);
  }

Swapchain::~Swapchain() {
  delete impl.handler;
  }

Swapchain& Swapchain::operator = (Swapchain&& s) {
  impl = std::move(s.impl);

  std::swap(api,                  s.api);
  std::swap(dev,                  s.dev);
  std::swap(hwnd,                 s.hwnd);
  std::swap(framesCounter,        s.framesCounter);
  std::swap(imgId,                s.imgId);
  std::swap(framesIdMod,          s.framesIdMod);
  std::swap(implMaxFramesInFlight,s.implMaxFramesInFlight);
  std::swap(img,                  s.img);

  return *this;
  }

uint32_t Swapchain::w() const {
  return impl.handler->w();
  }

uint32_t Swapchain::h() const {
  return impl.handler->h();
  }

void Swapchain::present(uint32_t img, const Semaphore& wait) {
  api->present(dev,impl.handler,img,wait.impl.handler);
  framesCounter++;
  framesIdMod=(framesIdMod+1)%maxFramesInFlight();
  }

void Swapchain::reset() {
  if(impl.handler!=nullptr) {
    impl.handler->reset();

    size_t cnt = imageCount();
    img.reset(new Attachment[cnt]);
    for(size_t i=0;i<cnt;++i) {
      img[i] = Attachment(impl.handler,i);
      }
    } else {
    impl = api->createSwapchain(hwnd,this->dev);
    }
  }

uint32_t Swapchain::imageCount() const {
  return impl ? impl.handler->imageCount() : 0;
  }

uint8_t Swapchain::maxFramesInFlight() const {
  return implMaxFramesInFlight;
  }

uint8_t Swapchain::frameId() const {
  return framesIdMod;
  }

Attachment& Swapchain::frame(uint32_t id) {
  return img[id];
  }

uint32_t Swapchain::nextImage(Semaphore& onReady) {
  imgId = impl.handler->nextImage(onReady.impl.handler);
  return imgId;
  }
