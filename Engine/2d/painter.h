#pragma once

#include <Tempest/PaintDevice>
#include <Tempest/Transform>
#include <Tempest/Font>
#include <Tempest/Brush>
#include <Tempest/Pen>

namespace Tempest {

class PaintEvent;

class Painter {
  public:
    enum Mode : uint8_t {
      Clear,
      Preserve
      };
    using Blend=PaintDevice::Blend;
    constexpr static auto NoBlend=Blend::NoBlend;
    constexpr static auto Alpha  =Blend::Alpha;
    constexpr static auto Add    =Blend::Add;

    Painter(PaintEvent& ev, Mode m=Preserve);
    Painter(const Painter&)=delete;
    ~Painter();
    Painter& operator = (const Painter&)=delete;

    void setScissor(int x,int y,int w,int h);
    void setScissor(int x,int y,unsigned w,unsigned h);
    void setScissor(const Rect& rect);

    void setBrush(const Brush& b);
    void setPen  (const Pen&   p);
    void setFont (const Font&  f);

    void translate(const Point& p);
    void translate(int x,int y);

    void rotate   (float angle);

    Rect         scissor() const { return Rect(s.scRect.x -s.scRect.ox,s.scRect.y -s.scRect.oy,
                                               s.scRect.x1-s.scRect.x, s.scRect.y1-s.scRect.y); }
    const Brush& brush()   const { return s.br;  }
    const Pen&   pen()     const { return s.pn;  }
    const Font&  font()    const { return s.fnt; }

    void pushState();
    void popState();

    void drawRect(int x,int y,int width,int height,
                  float u1,float v1,float u2,float v2);
    void drawRect(int x,int y,int width,int height,
                  int u1,int v1,int u2,int v2);
    void drawRect(int x,int y,int width,int height);
    void drawRect(int x,int y,unsigned width,unsigned height);
    void drawRect(const Rect& rect);

    void drawLine(int x1,int y1,int x2,int y2);
    void drawLine(const Point& a,const Point& b);

    void drawTriangle( int x0, int y0, float u0, float v0,
                       int x1, int y1, float u1, float v1,
                       int x2, int y2, float u2, float v2 );
    void drawTriangle( float x0, float y0, float u0, float v0,
                       float x1, float y1, float u1, float v1,
                       float x2, float y2, float u2, float v2 );

    void drawText(int x,int y,const char*     txt);
    void drawText(int x,int y,const char16_t* txt);

    void drawText(int x,int y,const std::string& txt);
    void drawText(int x,int y,const std::u16string& txt);

    void drawText(int x,int y,int w,int h,const char* txt,AlignFlag flg=NoAlign);
    void drawText(int x,int y,int w,int h,const std::string& txt,AlignFlag flg=NoAlign);

  private:
    enum State:uint8_t {
      StNo   =0,
      StBrush=1,
      StPen  =2
      };

    struct Tr {
      Tempest::Transform mat  = Transform();
      float              invW = 1.f;
      float              invH = 1.f;
      };

    struct ScissorRect {
      int ox=0,oy=0;
      int x=0,y=0,x1=0,y1=0;
      };

    struct InternalState {
      Tempest::Brush br;
      Tempest::Pen   pn;
      Tempest::Font  fnt;
      Tr             tr;
      ScissorRect    scRect;

      // sprite brush
      float invW    =1.f;
      float invH    =1.f;
      float dU      =0.f;
      float dV      =0.f;
      };

    PaintDevice&       dev;
    TextureAtlas&      ta;
    PaintDevice::Point pt;

    State              state=StNo;
    InternalState      s;
    std::vector<InternalState> stStk;

    struct FPoint{
      float x,y;
      float u,v;
      };

    void implBrush(const Brush& b);
    void implPen  (const Pen&   p);

    void implAddPoint(float x, float y, float u, float v);
    void implAddPoint(int   x, int   y, float u, float v);
    void implSetColor(float r,float g,float b,float a);

    void implDrawTrig( float x0, float y0, float u0, float v0,
                       float x1, float y1, float u1, float v1,
                       float x2, float y2, float u2, float v2,
                       FPoint *out, int stage);
    void implDrawRect(int x1, int y1, int x2, int y2,
                      float u1, float v1, float u2, float v2);

  friend class Font;
  };

}
