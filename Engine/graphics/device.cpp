#include "device.h"

#include <Tempest/Semaphore>
#include <Tempest/Except>

using namespace Tempest;

Device::Impl::Impl(AbstractGraphicsApi &api, AbstractGraphicsApi::Device *dev, SystemApi::Window *w, uint8_t maxFramesInFlight)
  :api(api),hwnd(w),maxFramesInFlight(maxFramesInFlight){
  swapchain=api.createSwapchain(w,dev);
  }

Device::Impl::~Impl() {
  delete swapchain;
  }

Device::Device(AbstractGraphicsApi &api, SystemApi::Window *w, uint8_t maxFramesInFlight)
  :HeadlessDevice(api,w), api(api), impl(api,implHandle(),w,maxFramesInFlight) {
  }

Device::~Device() {
  }

void Device::reset() {
  delete impl.swapchain;
  impl.swapchain=nullptr;
  impl.swapchain=api.createSwapchain(impl.hwnd,implHandle());
  }

uint32_t Device::swapchainImageCount() const {
  return impl.swapchain->imageCount();
  }

uint8_t Device::maxFramesInFlight() const {
  return impl.maxFramesInFlight;
  }

uint64_t Device::frameCounter() const {
  return framesCounter;
  }

uint32_t Device::imageId() const {
  return imgId;
  }

Frame Device::frame(uint32_t id) {
  Frame fr(*this,api.getImage(implHandle(),impl.swapchain,id),id);
  return fr;
  }

uint32_t Device::nextImage(Semaphore &onReady) {
  imgId = api.nextImage(implHandle(),impl.swapchain,onReady.impl.handler);
  return imgId;
  }

void Device::present(uint32_t img,const Semaphore &wait) {
  api.present(implHandle(),impl.swapchain,img,wait.impl.handler);
  framesCounter++;
  framesIdMod=(framesIdMod+1)%maxFramesInFlight();
  }

FrameBuffer Device::frameBuffer(Frame& out) {
  TextureFormat att[1] = {TextureFormat::Undefined};
  uint32_t w = impl.swapchain->w();
  uint32_t h = impl.swapchain->h();

  FrameBufferLayout lay(api.createFboLayout(implHandle(),w,h,impl.swapchain,att,1),w,h);
  FrameBuffer       f(*this,api.createFbo(implHandle(),lay.impl.handler,impl.swapchain,out.id),std::move(lay));
  return f;
  }

FrameBuffer Device::frameBuffer(Frame &out, Texture2d &zbuf) {
  TextureFormat att[2] = {TextureFormat::Undefined,zbuf.format()};
  uint32_t w = impl.swapchain->w();
  uint32_t h = impl.swapchain->h();

  if(int(w)!=zbuf.w() || int(h)!=zbuf.h())
    throw IncompleteFboException();

  FrameBufferLayout lay(api.createFboLayout(implHandle(),w,h,impl.swapchain,att,2),w,h);
  FrameBuffer       f(*this,api.createFbo(implHandle(),lay.impl.handler,impl.swapchain,out.id,zbuf.impl.handler),std::move(lay));
  return f;
  }
