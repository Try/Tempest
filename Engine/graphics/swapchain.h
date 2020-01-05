#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class Device;
class Semaphore;
class Frame;

class Swapchain {
  public:
    Swapchain(SystemApi::Window* w, Device& dev);
    Swapchain(Swapchain&&)=default;
    virtual ~Swapchain();

    Swapchain& operator = (Swapchain&& s);

    uint32_t             w() const;
    uint32_t             h() const;

    void                 present(uint32_t img,const Semaphore& wait);
    void                 reset();

    uint32_t             imageCount() const;
    uint8_t              maxFramesInFlight() const;
    uint8_t              frameId() const;
    uint64_t             frameCounter() const;

    Frame                frame(uint32_t id);
    uint32_t             nextImage(Semaphore& onReady);

  private:
    Swapchain(SystemApi::Window* w, AbstractGraphicsApi::Device& dev, AbstractGraphicsApi& api, uint8_t maxFramesInFlight);

    Detail::DPtr<AbstractGraphicsApi::Swapchain*> impl;
    AbstractGraphicsApi*                          api =nullptr;
    AbstractGraphicsApi::Device*                  dev =nullptr;
    SystemApi::Window*                            hwnd=nullptr;

    uint64_t                                      framesCounter=0;
    uint32_t                                      imgId=0;
    uint8_t                                       framesIdMod=0;
    uint8_t                                       implMaxFramesInFlight=0;

  friend class Device;
  };

}

