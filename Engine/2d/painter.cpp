#include "painter.h"

#include <Tempest/PaintDevice>
#include <Tempest/Event>

using namespace Tempest;

Painter::Painter(PaintEvent &ev, Mode m) : dev(ev.device()) {
  float w=2.f/ev.w();
  float h=2.f/ev.h();
  tr = Transform(w,0,0,
                 0,h,0,
                 -1,-1, 1);
  implSetColor(1,1,1,1);
  dev.beginPaint(m==Clear,ev.w(),ev.h());
  }

Painter::~Painter() {
  dev.endPaint();
  }

void Painter::implAddPoint(int x, int y, float u, float v) {
  tr.map(x,y,pt.x,pt.y);
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

void Painter::setBrush(const Brush& b) {
  dev.setBrush(b.tex,1,1,1,1);
  invW=1.f/b.w();
  invH=1.f/b.h();
  }

void Painter::drawRect(int x, int y, int w, int h) {
  dev.setTopology(Triangles);
  implAddPoint(x,  y,  0,     0);
  implAddPoint(x+w,y,  w*invW,0);
  implAddPoint(x+w,y+h,w*invW,h*invH);

  implAddPoint(x,  y,  0,     0);
  implAddPoint(x+w,y+h,w*invW,h*invH);
  implAddPoint(x,  y+h,0,     h*invH);
  }

void Painter::drawLine(int x1, int y1, int x2, int y2) {
  dev.setTopology(Lines);
  implAddPoint(x1,y1,0,0);
  implAddPoint(x2,y2,x2-x1,y2-y1);
  }
