#include "painter.h"

#include <Tempest/Application>
#include <Tempest/PaintDevice>
#include <Tempest/Event>
#include <Tempest/Brush>
#include <Tempest/Pen>

#include "../utility/utf8_helper.h"

using namespace Tempest;

Painter::Painter(PaintEvent &ev, Mode m)
  : dev(ev.device()), ta(ev.ta), fnt(Application::font()) {
  const Point& dp=ev.orign();
  tr.mat = Transform(1,0,0,
                     0,1,0,
                     float(dp.x), float(dp.y), 1);
  tr.invW = 2.f/(ev.w());
  tr.invH = 2.f/(ev.h());

  const Rect&  r = ev.viewPort();
  scRect.ox = dp.x;
  scRect.oy = dp.y;
  setScissor(r.x,r.y,r.w,r.h);

  implSetColor(1,1,1,1);
  dev.beginPaint(m==Clear,ev.w(),ev.h());
  }

Painter::~Painter() {
  dev.endPaint();
  }

void Painter::setScissor(int x, int y, int w, int h) {
  scRect.x  = scRect.ox+x;
  scRect.y  = scRect.oy+y;
  scRect.x1 = scRect.ox+x+w;
  scRect.y1 = scRect.oy+y+h;

  if(scRect.x>scRect.x1)
    std::swap(scRect.x,scRect.x1);
  if(scRect.y>scRect.y1)
    std::swap(scRect.y,scRect.y1);
  }

void Painter::setScissor(int x, int y, unsigned w, unsigned h) {
  scRect.x  = x;
  scRect.y  = y;
  scRect.x1 = x+int(w);
  scRect.y1 = y+int(h);
  }

void Painter::setScissor(const Rect& r) {
  setScissor(r.x,r.y,r.w,r.h);
  }

void Painter::implAddPoint(float x, float y, float u, float v) {
  tr.mat.map(x,y,pt.x,pt.y);
  pt.x = pt.x*tr.invW-1.f;
  pt.y = pt.y*tr.invH-1.f;
  pt.u=u;
  pt.v=v;
  dev.addPoint(pt);
  }

void Painter::implAddPointRaw(float x, float y, float u, float v) {
  pt.x=x*tr.invW-1.f;
  pt.y=y*tr.invH-1.f;
  pt.u=u;
  pt.v=v;
  dev.addPoint(pt);
  }

void Painter::implAddPointRaw(int x, int y, float u, float v) {
  pt.x=x*tr.invW-1.f;
  pt.y=y*tr.invH-1.f;
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

void Painter::drawTriangle(int x0, int y0, float u0, float v0,
                           int x1, int y1, float u1, float v1,
                           int x2, int y2, float u2, float v2) {
  FPoint trigBuf[4+4+4+4];
  drawTrigImpl( float(x0), float(y0), u0, v0,
                float(x1), float(y1), u1, v1,
                float(x2), float(y2), u2, v2,
                trigBuf, 0 );
  }

void Painter::drawTriangle(float x0, float y0, float u0, float v0,
                           float x1, float y1, float u1, float v1,
                           float x2, float y2, float u2, float v2) {
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
  ScissorRect& s = scRect;
  FPoint*      r = out;

  float x[4] = {x0,x1,x2,x0};
  float y[4] = {y0,y1,y2,y0};
  float u[4] = {u0,u1,u2,u0};
  float v[4] = {v0,v1,v2,v0};

  float mid;
  float sx = 0.f;
  bool  cs = false, ns = false;

  switch( stage ) {
    case 0: sx = float(s.x);  break;
    case 1: sx = float(s.y);  break;
    case 2: sx = float(s.x1); break;
    case 3: sx = float(s.y1); break;
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

  ptrdiff_t count = out-r;
  count-=3;

  if( stage<3 ){
    for(ptrdiff_t i=0;i<=count;++i){
      drawTrigImpl( r[  0].x, r[  0].y, r[  0].u, r[  0].v,
          r[i+1].x, r[i+1].y, r[i+1].u, r[i+1].v,
          r[i+2].x, r[i+2].y, r[i+2].u, r[i+2].v,
          out, stage+1 );
      }
    } else {
    for(ptrdiff_t i=0;i<=count;++i){
      implAddPointRaw(r[  0].x, r[  0].y, r[  0].u, r[  0].v);
      implAddPointRaw(r[i+1].x, r[i+1].y, r[i+1].u, r[i+1].v);
      implAddPointRaw(r[i+2].x, r[i+2].y, r[i+2].u, r[i+2].v);
      }
    }
  }

void Painter::setBrush(const Brush& b) {
  if(state==StBrush)
    implBrush(b);

  bru=b;
  invW=b.info.invW;
  invH=b.info.invH;
  dU  =b.info.dx;
  dV  =b.info.dy;
  }

void Painter::setPen(const Pen &p) {
  if(state==StPen)
    implPen(p);
  pn=p;
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
    state=StBrush;
    implBrush(bru);
    }

  if(T_LIKELY(tr.mat.type()==Transform::T_AxisAligned)) {
    tr.mat.map(x1,y1, x1,y1);
    tr.mat.map(x2,y2, x2,y2);
    if(T_UNLIKELY(x1>x2)) {
      std::swap(x1,x2);
      std::swap(u1,u2);
      }
    if(T_UNLIKELY(y1>y2)) {
      std::swap(y1,y2);
      std::swap(v1,v2);
      }

    if(x1<scRect.x){
      int dx=scRect.x-x1;
      x1+=dx;
      if(x1>=x2)
        return;
      u1+=dx*invW;
      }

    if(scRect.x1<x2){
      int dx=scRect.x1-x2;
      x2+=dx;
      if(x1>=x2)
        return;
      u2+=dx*invW;
      }

    if(y1<scRect.y){
      int dy=scRect.y-y1;
      y1+=dy;
      if(y1>=y2)
        return;
      v1+=dy*invH;
      }

    if(scRect.y1<y2){
      int dy=scRect.y1-y2;
      y2+=dy;
      if(y1>=y2)
        return;
      v2+=dy*invH;
      }

    implAddPointRaw(x1,y1, u1,v1);
    implAddPointRaw(x2,y1, u2,v1);
    implAddPointRaw(x2,y2, u2,v2);

    implAddPointRaw(x1,y1, u1,v1);
    implAddPointRaw(x2,y2, u2,v2);
    implAddPointRaw(x1,y2, u1,v2);
    } else {
    float x[4] = {float(x1), float(x2), float(x2), float(x1)};
    float y[4] = {float(y1), float(y1), float(y2), float(y2)};
    for(size_t i=0;i<4;++i)
      tr.mat.map(x[i],y[i],x[i],y[i]);

    drawTriangle(x[0],y[0], u1,v1,
                 x[1],y[1], u2,v1,
                 x[2],y[2], u2,v2);
    drawTriangle(x[0],y[0], u1,v1,
                 x[2],y[2], u2,v2,
                 x[3],y[3], u1,v2);
    }
  }

void Painter::drawRect(int x, int y, int w, int h, float u1, float v1, float u2, float v2) {
  implDrawRect(x,y,x+w,y+h, dU+u1*invW,dV+v1*invH,u2*invW+dU,v2*invH+dV);
  }

void Painter::drawRect(int x, int y, int w, int h, int u1, int v1, int u2, int v2) {
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
    state=StPen;
    implPen(pn);
    }
  /*
  if(penWidth>1.f){
    float dx=-(y2-y1),dy=x2-x1;
    return;
    }*/

  implAddPoint(x1+0.5f,y1+0.5f,0,0);
  implAddPoint(x2+0.5f,y2+0.5f,x2-x1,y2-y1);
  }

void Painter::setFont(const Font &f) {
  fnt=f;
  }

void Painter::translate(const Point& p) {
  tr.mat.translate(p);
  }

void Painter::translate(int x, int y) {
  tr.mat.translate(float(x),float(y));
  }

void Painter::rotate(float angle) {
  tr.mat.rotate(angle);
  }

void Painter::drawText(int x, int y, const char *txt) {
  if(txt==nullptr)
    return;
  auto pb=bru;
  Utf8Iterator i(txt);
  while(i.hasData()) {
    auto c=i.next();
    if(c=='\0'){
      setBrush(pb);
      return;
      }
    auto l=fnt.letter(c,ta);

    if(!l.view.isEmpty()) {
      setBrush(Brush(l.view,pb.color,PaintDevice::Alpha));
      drawRect(x+l.dpos.x,y+l.dpos.y,l.view.w(),l.view.h());
      }

    x += l.advance.x;
    }
  setBrush(pb);
  }

void Painter::drawText(int x, int y, const char16_t *txt) {
  if(txt==nullptr)
    return;
  auto pb=bru;

  for(;*txt;++txt) {
    auto& l=fnt.letter(*txt,ta);

    if(!l.view.isEmpty()) {
      setBrush(Brush(l.view,pb.color,pb.blend));
      drawRect(x+l.dpos.x,y+l.dpos.y,l.view.w(),l.view.h());
      }

    x += l.advance.x;
    }

  setBrush(pb);
  }

void Painter::drawText(int x, int y, const std::string &txt) {
  return drawText(x,y,txt.c_str());
  }

void Painter::drawText(int x, int y, const std::u16string &txt) {
  return drawText(x,y,txt.c_str());
  }

static Utf8Iterator advanceByWord(Utf8Iterator i, int& x, int w, Font& fnt, TextureAtlas& ta){
  while(true){
    auto c=i.peek();
    if(c=='\0' || c=='\n' || c=='\r' || c=='\t' || c=='\v' || c=='\f' || c==' ')
      break;

    i.next();
    auto l=fnt.letter(c,ta);
    x += l.advance.x;
    if(x>=w)
      break;
    }
  return i;
  }

static Utf8Iterator advanceByLine(Utf8Iterator i, int& x, int w, Font& fnt, TextureAtlas& ta){
  size_t wordCount=0;
  while(true){
    Utf8Iterator eow = advanceByWord(i,x,w,fnt,ta);
    ++wordCount;

    if(x>=w && wordCount>1)
      break;
    auto c=eow.next();
    if(c=='\0' || c=='\n'){
      i = eow;
      break;
      }
    auto l=fnt.letter(c,ta);
    if(x+l.dpos.x+l.view.w()>=w && wordCount>1)
      break;
    x += l.advance.x;
    i = eow;
    }
  return i;
  }

void Painter::drawText(int rx, int ry, int w, int /*h*/, const char *txt, AlignFlag flg) {
  if(txt==nullptr)
    return;
  auto pb=bru;
  int  x =0, y=0;
  Utf8Iterator i(txt);

  while(i.hasData()) {
    // make next line
    x = 0;
    Utf8Iterator eol = advanceByLine(i,x,w,fnt,ta);
    if(i==eol)
      break;

    x = 0;
    while(i!=eol){
      auto c=i.next();
      if(c=='\0'){
        setBrush(pb);
        return;
        }
      if(c=='\n' || c=='\r')
        continue;
      auto l=fnt.letter(c,ta);

      if(!l.view.isEmpty()) {
        setBrush(Brush(l.view,pb.color,PaintDevice::Alpha));
        drawRect(rx+x+l.dpos.x,
                 ry+y+l.dpos.y,
                 l.view.w(),l.view.h());
        }

      x += l.advance.x;
      }
    y+=int(std::ceil(fnt.pixelSize()));
    }
  setBrush(pb);
  }

void Painter::drawText(int x, int y, int w, int h, const std::string &txt, AlignFlag flg) {
  drawText(x,y,w,h,txt.c_str(),flg);
  }

