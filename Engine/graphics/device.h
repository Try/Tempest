#pragma once

#include <Tempest/HeadlessDevice>

namespace Tempest {

class Device : public HeadlessDevice {
  public:
    using Caps=AbstractGraphicsApi::Caps;

    Device(AbstractGraphicsApi& api,SystemApi::Window* w,uint8_t maxFramesInFlight=2);
    Device(const Device&)=delete;
    virtual ~Device();

    void                 present(uint32_t img,const Semaphore& wait);
    void                 reset();

    uint32_t             swapchainImageCount() const;
    uint8_t              maxFramesInFlight() const;
    uint8_t              frameId() const;
    uint64_t             frameCounter() const;

    Frame                frame(uint32_t id);
    uint32_t             nextImage(Semaphore& onReady);

    using                HeadlessDevice::frameBuffer;
    FrameBuffer          frameBuffer(Frame &out);
    FrameBuffer          frameBuffer(Frame &out, Texture2d& zbuf);

  private:
    struct Impl {
      Impl(AbstractGraphicsApi& api, AbstractGraphicsApi::Device* dev, SystemApi::Window *w, uint8_t maxFramesInFlight);
      ~Impl();

      AbstractGraphicsApi&            api;
      AbstractGraphicsApi::Swapchain* swapchain=nullptr;
      SystemApi::Window*              hwnd=nullptr;
      const uint8_t                   maxFramesInFlight=1;
      };

    AbstractGraphicsApi&            api;
    Impl                            impl;

    uint64_t                        framesCounter=0;
    uint8_t                         framesIdMod=0;
    uint32_t                        imgId=0;
  };

inline uint8_t Device::frameId() const {
  return framesIdMod;
  }
}
