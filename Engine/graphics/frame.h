#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class Device;
class CommandBuffer;

class Frame {
  public:
    Frame();
    Frame(Frame&& f);
    ~Frame();

  private:
    Frame(Tempest::Device& dev,AbstractGraphicsApi::Image* img,uint32_t id);

    Tempest::Device*            dev=nullptr;
    AbstractGraphicsApi::Image* img=nullptr;
    uint32_t                    id =0;

  friend class Tempest::CommandBuffer;
  friend class Tempest::Device;
  };

}
