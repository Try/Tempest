#include "painter.h"

#include <Tempest/PaintDevice>
#include <Tempest/Event>
#include <Tempest/Brush>
#include <Tempest/Pen>

using namespace Tempest;

Painter::Painter(PaintEvent &ev, Mode m)
  : dev(ev.device()), ta(ev.ta) {
  float w=2.f/ev.w();
  float h=2.f/ev.h();
  const Rect& r=ev.viewPort();
  tr = Transform(w,0,0,
                 0,h,0,
                 -1+r.x*w,-1+r.y*h, 1);

  implSetColor(1,1,1,1);
  setScissot(0,0,ev.w(),ev.h());
  dev.beginPaint(m==Clear,ev.w(),ev.h());
  }

Painter::~Painter() {
  dev.endPaint();
  }

void Painter::setScissot(int x, int y, int w, int h) {
  scissor.x  = x;
  scissor.y  = y;
  scissor.x1 = x+w;
  scissor.y1 = y+h;

  if(scissor.x>scissor.x1)
    std::swap(scissor.x,scissor.x1);
  if(scissor.y>scissor.y1)
    std::swap(scissor.y,scissor.y1);
  }

void Painter::setScissot(int x, int y, unsigned w, unsigned h) {
  scissor.x  = x;
  scissor.y  = y;
  scissor.x1 = x+int(w);
  scissor.y1 = y+int(h);
  }

void Painter::implAddPoint(int x, int y, float u, float v) {
  tr.map(x,y,pt.x,pt.y);
  pt.u=u;
  pt.v=v;
  dev.addPoint(pt);
  }

void Painter::implAddPointRaw(float x, float y, float u, float v) {
  pt.x=x;
  pt.y=y;
  pt.u=u;
  pt.v=v;
  dev.addPoint(pt);
  }

void Painter::implSetColor(float r, float g, float b, float a) {
  pt.r=r;
  pt.g=g;
  pt.b=b;
  pt.a=a;
  }

void Painter::drawTriangle(int x0, int y0, int u0, int v0,
                           int x1, int y1, int u1, int v1,
                           int x2, int y2, int u2, int v2) {
  FPoint trigBuf[4+4+4+4];
  drawTrigImpl( x0, y0, u0, v0,
                x1, y1, u1, v1,
                x2, y2, u2, v2,
                trigBuf, 0 );
  }

void Painter::drawTrigImpl( float x0, float y0, float u0, float v0,
                            float x1, float y1, float u1, float v1,
                            float x2, float y2, float u2, float v2,
                            FPoint* out,
                            int stage ) {
  ScissorRect& s = scissor;
  FPoint*      r = out;

  float x[4] = {x0,x1,x2,x0};
  float y[4] = {y0,y1,y2,y0};
  float u[4] = {u0,u1,u2,u0};
  float v[4] = {v0,v1,v2,v0};

  float mid;
  float sx = 0.f;
  bool  cs = false, ns = false;

  switch( stage ) {
    case 0: {
    sx = s.x;
    for(int i=0;i<4;++i){
      tr.map(x[i],y[i],x[i],y[i]);
      }
    }
      break;
    case 1: sx = s.y;  break;
    case 2: sx = s.x1; break;
    case 3: sx = s.y1; break;
    }

  for(int i=0;i<3;++i){
    switch( stage ){
      case 0:
        cs = x[i]  >=sx;
        ns = x[i+1]>=sx;
        break;
      case 1:
        cs = y[i]  >=sx;
        ns = y[i+1]>=sx;
        break;
      case 2:
        cs = x[i]  <sx;
        ns = x[i+1]<sx;
        break;
      case 3:
        cs = y[i]  <sx;
        ns = y[i+1]<sx;
        break;
      }

    if(cs==ns){
      if( cs ){
        *out = FPoint{float(x[i+1]),float(y[i+1]),float(u[i+1]),float(v[i+1])};
        ++out;
        }
      } else {
      float dx = (x[i+1]-x[i]);
      float dy = (y[i+1]-y[i]);
      float du = (u[i+1]-u[i]);
      float dv = (v[i+1]-v[i]);
      if( ns ){
        if(stage%2==0){
          mid = y[i] + (dy*(sx-x[i]))/dx;
          *out = FPoint{sx, mid,
              u[i] + (du*(sx-x[i]))/dx,
              v[i] + (dv*(sx-x[i]))/dx};   ++out;

          *out = FPoint{float(x[i+1]),float(y[i+1]),float(u[i+1]),float(v[i+1])};
          ++out;
          } else {
          mid = x[i] + (dx*(sx-y[i]))/dy;
          *out = FPoint{mid, sx,
              u[i] + (du*(sx-y[i]))/dy,
              v[i] + (dv*(sx-y[i]))/dy};   ++out;

          *out = FPoint{float(x[i+1]),float(y[i+1]),float(u[i+1]),float(v[i+1])};
          ++out;
          }
        } else {
        if(stage%2==0){
          mid = y[i] + (dy*(sx-x[i]))/dx;
          *out = FPoint{sx, mid,
              u[i] + (du*(sx-x[i]))/dx,
              v[i] + (dv*(sx-x[i]))/dx};   ++out;
          } else {
          mid = x[i] + (dx*(sx-y[i]))/dy;
          *out = FPoint{mid, sx,
              u[i] + (du*(sx-y[i]))/dy,
              v[i] + (dv*(sx-y[i]))/dy};   ++out;
          }
        }
      }
    cs = ns;
    }

  int count = out-r;
  count-=3;

  if( stage<3 ){
    for(int i=0;i<=count;++i){
      drawTrigImpl( r[  0].x, r[  0].y, r[  0].u, r[  0].v,
          r[i+1].x, r[i+1].y, r[i+1].u, r[i+1].v,
          r[i+2].x, r[i+2].y, r[i+2].u, r[i+2].v,
          out, stage+1 );
      }
    } else {
    for(int i=0;i<=count;++i){
      implAddPointRaw(r[  0].x, r[  0].y, r[  0].u, r[  0].v);
      implAddPointRaw(r[i+1].x, r[i+1].y, r[i+1].u, r[i+1].v);
      implAddPointRaw(r[i+2].x, r[i+2].y, r[i+2].u, r[i+2].v);
      }
    }
  }

void Painter::setBrush(const Brush& b) {
  if(state==StBrush)
    implBrush(b);

  brush=b;
  invW=b.info.invW;
  invH=b.info.invH;
  dU  =b.info.dx;
  dV  =b.info.dy;
  }

void Painter::setPen(const Pen &p) {
  if(state==StPen)
    implPen(p);
  pen=p;
  }

void Painter::implBrush(const Brush &b) {
  if(b.tex) {
    dev.setState(b.tex,b.color);
    } else {
    dev.setState(b.spr,b.color);
    }
  dev.setBlend(b.blend);
  implSetColor(b.color.r(),b.color.g(),b.color.b(),b.color.a());
  }

void Painter::implPen(const Pen &p) {
  dev.setState(Brush::TexPtr(),p.color);
  dev.setBlend(Blend::NoBlend);
  implSetColor(p.color.r(),p.color.g(),p.color.b(),p.color.a());
  }

void Painter::implDrawRect(int x1, int y1, int x2, int y2, float u1, float v1, float u2, float v2) {
  if(state!=StBrush) {
    dev.setTopology(Triangles);
    implBrush(brush);
    }

  if(x1<scissor.x){
    int dx=scissor.x-x1;
    x1+=dx;
    u1+=dx*invW;
    }
  if(scissor.x1<x2){
    int dx=scissor.x1-x2;
    x2+=dx;
    u2+=dx*invW;
    }
  if(y1<scissor.y){
    int dy=scissor.y-y1;
    y1+=dy;
    v1+=dy*invH;
    }
  if(scissor.y1<y2){
    int dy=scissor.y1-y2;
    y2+=dy;
    v2+=dy*invW;
    }
  implAddPoint(x1,y1, u1,v1);
  implAddPoint(x2,y1, u2,v1);
  implAddPoint(x2,y2, u2,v2);

  implAddPoint(x1,y1, u1,v1);
  implAddPoint(x2,y2, u2,v2);
  implAddPoint(x1,y2, u1,v2);
  }

void Painter::drawRect(int x, int y, int w, int h, float u1, float v1, float u2, float v2) {
  implDrawRect(x,y,x+w,y+h, dU+u1*invW,dV+v1*invH,u2*invW+dU,v2*invH+dV);
  }

void Painter::drawRect(int x, int y, int w, int h) {
  implDrawRect(x,y,x+w,y+h, dU,dV,w*invW+dU,h*invH+dV);
  }

void Painter::drawRect(int x, int y, unsigned w, unsigned h) {
  implDrawRect(x,y,x+int(w),y+int(h), dU,dV,w*invW+dU,h*invH+dV);
  }

void Painter::drawLine(int x1, int y1, int x2, int y2) {
  if(state!=StPen){
    dev.setTopology(Lines);
    implPen(pen);
    }
  /*
  if(penWidth>1.f){
    float dx=-(y2-y1),dy=x2-x1;
    return;
    }*/
  implAddPoint(x1,y1,0,0);
  implAddPoint(x2,y2,x2-x1,y2-y1);
  }

void Painter::setFont(const Font &f) {
  fnt=f;
  }

void Painter::drawText(int x, int y, const char16_t *txt) {
  auto pb=brush;

  for(;*txt;++txt) {
    auto& l=fnt.letter(*txt,ta);

    if(!l.view.isEmpty()) {
      setBrush(l.view);
      drawRect(x+l.dpos.x,y+l.dpos.y,l.view.w(),l.view.h());
      }

    x += l.advance.x;
    }

  setBrush(pb);
  }
