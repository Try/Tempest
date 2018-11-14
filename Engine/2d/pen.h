#pragma once

#include <Tempest/Color>

namespace Tempest {

class Pen {
  public:
    Pen(const Color& cl,float w=1.f);

    float width() const { return penW; }

  private:
    Color cl;
    float penW=1.f;
  };

}
