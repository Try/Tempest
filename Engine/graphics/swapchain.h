#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class Device;
class Semaphore;
class Frame;
class Attachment;

class Swapchain final {
  public:
    Swapchain(Device& dev, SystemApi::Window* w);
    Swapchain(Swapchain&&)=default;
    ~Swapchain();

    Swapchain& operator = (Swapchain&& s);

    uint32_t             w() const;
    uint32_t             h() const;

    void                 present(uint32_t img,const Semaphore& wait);
    void                 reset();

    uint32_t             imageCount() const;
    uint8_t              maxFramesInFlight() const;
    uint8_t              frameId() const;
    uint64_t             frameCounter() const;

    Attachment           frame(uint32_t id);
    uint32_t             nextImage(Semaphore& onReady);

  private:
    Swapchain(AbstractGraphicsApi::Device& dev, AbstractGraphicsApi& api, SystemApi::Window* w, uint8_t maxFramesInFlight);

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

