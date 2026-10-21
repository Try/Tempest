#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Attachment>

namespace Tempest {

class Device;
class Frame;

class Swapchain final {
  public:
    Swapchain(Device& dev, SystemApi::Window* w);
    Swapchain(Swapchain&&)=default;
    ~Swapchain();

    Swapchain& operator = (Swapchain&& s);

    uint32_t             w() const;
    uint32_t             h() const;

    void                 reset();

    // Returns the same image until present or reset. Currently supported by Metal.
    // The image belongs to the swapchain; do not move from it.
    // The reference must not be used after present, reset or swapchain destruction.
    Attachment&          acquire();

    uint32_t             currentImage() const;
    uint32_t             imageCount() const;
    Attachment&          operator[](size_t id);
    const Attachment&    operator[](size_t id) const;

  private:
    Swapchain(AbstractGraphicsApi::Swapchain* sw);

    void implReset();

    Detail::DPtr<AbstractGraphicsApi::Swapchain*> impl;
    std::unique_ptr<Attachment[]>                 img;
    Attachment                                    acquired;

  friend class Device;
  };

}

