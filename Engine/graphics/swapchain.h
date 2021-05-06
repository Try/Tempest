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

    void                 reset();

    uint32_t             imageCount() const;
    uint8_t              frameId() const;
    uint64_t             frameCounter() const;

    Attachment&          frame(size_t id);
    uint32_t             nextImage(Semaphore& onReady);

  private:
    Swapchain(AbstractGraphicsApi::Swapchain* sw);

    void implReset();

    Detail::DPtr<AbstractGraphicsApi::Swapchain*> impl;

    uint64_t                                      framesCounter=0;
    uint8_t                                       framesIdMod=0;
    std::unique_ptr<Attachment[]>                 img;

  friend class Device;
  };

}

