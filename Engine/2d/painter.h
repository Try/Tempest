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
    void drawLine(int x1,int y1,int x2,int y2);

  private:
    PaintDevice&       dev;
    PaintDevice::Point pt;
    Tempest::Transform tr=Transform();

    float invW=1.f;
    float invH=1.f;

    void implAddPoint(int x, int y, float u, float v);
    void implSetColor(float r,float g,float b,float a);
  };

}
