#pragma once

#include <Tempest/Color>
#include <Tempest/PaintDevice>

namespace Tempest {

class Pen {
  public:
    Pen()=default;
    Pen(const Color& cl, PaintDevice::Blend blend=PaintDevice::Alpha, float w=1.f);

    float width() const { return penW; }

  private:
    Color              color;
    PaintDevice::Blend blend = PaintDevice::NoBlend;
    float              penW=1.f;

  friend class Painter;
  };

}
