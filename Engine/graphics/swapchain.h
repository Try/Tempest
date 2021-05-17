#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class Device;
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
    Attachment&          image(size_t id);
    uint32_t             currentImage() const;

  private:
    Swapchain(AbstractGraphicsApi::Swapchain* sw);

    void implReset();

    Detail::DPtr<AbstractGraphicsApi::Swapchain*> impl;
    std::unique_ptr<Attachment[]>                 img;

  friend class Device;
  };

}

