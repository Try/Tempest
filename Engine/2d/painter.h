#pragma once

#include <Tempest/Brush>
#include <Tempest/PaintDevice>
#include <Tempest/Transform>

namespace Tempest {

class PaintEvent;

class Painter {
  public:
    enum Mode {
      Clear,
      Preserve
      };
    Painter(PaintEvent& ev, Mode m=Clear);
    ~Painter();

    void setBrush(const Brush& b);
    void drawRect(int x,int y,int width,int height);

  private:
    PaintDevice&       dev;
    PaintDevice::Point pt;
    Tempest::Transform tr=Transform();

    void implAddPoint(int x, int y, float u, float v);
    void implSetColor(float r,float g,float b,float a);
  };

}
