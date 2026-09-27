#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class Device;
class Frame;
class Attachment;

class Swapchain final {
  public:
    using RenderMode = AbstractGraphicsApi::Swapchain::RenderMode;
    using Options    = AbstractGraphicsApi::Swapchain::Options;

    Swapchain(Device& dev, SystemApi::Window* w);
    // Metal: bufferCount 0 preserves the default, 2/3 selects the drawable pool.
    // Direct prefers rendering without a copy. Other backends ignore these hints.
    Swapchain(Device& dev, SystemApi::Window* w, const Options& options);
    Swapchain(Swapchain&&)=default;
    ~Swapchain();

    Swapchain& operator = (Swapchain&& s);

    uint32_t             w() const;
    uint32_t             h() const;

    void                 reset();

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

