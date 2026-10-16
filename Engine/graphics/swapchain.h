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

    // Requests a display callback rate, not a guaranteed rendering rate.
    // Zero restores the system default. Ignored on unsupported platforms.
    void                 setPreferredFrameRate(uint32_t fps);

    // Rates are limited to the screen's maximum; minimum and preferred are
    // clamped to the resulting range. Zero minimum means 1, zero preferred
    // leaves the choice to the system, and zero maximum restores the default.
    void                 setPreferredFrameRateRange(uint32_t minimum, uint32_t maximum, uint32_t preferred = 0);

    uint32_t             currentImage() const;
    uint32_t             imageCount() const;
    Attachment&          operator[](size_t id);
    const Attachment&    operator[](size_t id) const;

  private:
    Swapchain(AbstractGraphicsApi::Swapchain* sw);

    void implReset();

    Detail::DPtr<AbstractGraphicsApi::Swapchain*> impl;
    std::unique_ptr<Attachment[]>                 img;

  friend class Device;
  };

}

