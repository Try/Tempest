#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class Device;
class CommandBuffer;
class Swapchain;

class Frame final {
  public:
    Frame();
    Frame(Frame&& f);
    ~Frame();

  private:
    Frame(AbstractGraphicsApi::Image* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id);

    AbstractGraphicsApi::Image*     img       = nullptr;
    AbstractGraphicsApi::Swapchain* swapchain = nullptr;
    uint32_t                        id        = 0;

  friend class Tempest::CommandBuffer;
  friend class Tempest::Device;
  friend class Tempest::Swapchain;
  };

}
