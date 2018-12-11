#pragma once

#include <Tempest/Color>

namespace Tempest {

class Pen {
  public:
    Pen()=default;
    Pen(const Color& cl,float w=1.f);

    float width() const { return penW; }

  private:
    Color color;
    float penW=1.f;

  friend class Painter;
  };

}
