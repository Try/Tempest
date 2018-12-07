#pragma once

#include <Tempest/PaintDevice>
#include <Tempest/Transform>
#include <Tempest/Font>

namespace Tempest {

class PaintEvent;
class Brush;
class Pen;

class Painter {
  public:
    enum Mode : uint8_t {
      Clear,
      Preserve
      };
    using Blend=PaintDevice::Blend;
    constexpr static auto NoBlend=Blend::NoBlend;
    constexpr static auto Alpha  =Blend::Alpha;

    Painter(PaintEvent& ev, Mode m=Preserve);
    ~Painter();

    void setScissot(int x,int y,int w,int h);
    void setScissot(int x,int y,unsigned w,unsigned h);

    void setBrush(const Brush& b);
    void setPen  (const Pen&   p);
    void setFont (const Font&  f);
    void setBlend(const Blend  b);

    const Font& font() const { return fnt; }

    void drawRect(int x,int y,int width,int height,
                  float u1,float v1,float u2,float v2);
    void drawRect(int x,int y,int width,int height);
    void drawRect(int x,int y,unsigned width,unsigned height);

    void drawLine(int x1,int y1,int x2,int y2);

    void drawText(int x,int y,const char16_t* txt);

  private:
    PaintDevice&       dev;
    TextureAtlas&      ta;
    PaintDevice::Point pt;
    Tempest::Font      fnt;
    Tempest::Transform tr=Transform();

    float invW    =1.f;
    float invH    =1.f;
    float dU      =0.f;
    float dV      =0.f;
    float penWidth=1.f;

    struct ScissorRect {
      int x=0,y=0,x1=0,y1=0;
      } scissor;

    struct FPoint{
      float x,y;
      float u,v;
      };

    void implAddPoint   (int x, int y, float u, float v);
    void implAddPointRaw(float x, float y, float u, float v);
    void implSetColor   (float r,float g,float b,float a);

    void drawTriangle( int x0, int y0, int u0, int v0,
                       int x1, int y1, int u1, int v1,
                       int x2, int y2, int u2, int v2 );
    void drawTrigImpl( float x0, float y0, float u0, float v0,
                       float x1, float y1, float u1, float v1,
                       float x2, float y2, float u2, float v2,
                       FPoint *out, int stage);
    void implDrawRect(int x1, int y1, int x2, int y2,
                      float u1, float v1, float u2, float v2);
  };

}
