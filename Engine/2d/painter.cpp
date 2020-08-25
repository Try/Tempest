#include "painter.h"

#include <Tempest/Application>
#include <Tempest/PaintDevice>
#include <Tempest/Event>
#include <Tempest/Brush>
#include <Tempest/Pen>

#include "../utility/utf8_helper.h"

using namespace Tempest;

Painter::Painter(PaintEvent &ev, Mode m)
  : dev(ev.device()), ta(ev.ta) {
  s.fnt = Application::font();
  const Point& dp=ev.orign();
  s.tr.mat = Transform(1,0,0,
                       0,1,0,
                       float(dp.x), float(dp.y), 1);
  s.tr.invW = 2.f/(ev.w());
  s.tr.invH = 2.f/(ev.h());

  const Rect&  r = ev.viewPort();
  s.scRect.ox = dp.x;
  s.scRect.oy = dp.y;
  setScissor(r.x,r.y,r.w,r.h);

  implSetColor(1,1,1,1);
  dev.beginPaint(m==Clear,ev.w(),ev.h());
  }

Painter::~Painter() {
  dev.endPaint();
  }

void Painter::setScissor(int x, int y, int w, int h) {
  s.scRect.x  = s.scRect.ox+x;
  s.scRect.y  = s.scRect.oy+y;
  s.scRect.x1 = s.scRect.ox+x+w;
  s.scRect.y1 = s.scRect.oy+y+h;

  if(s.scRect.x>s.scRect.x1)
    std::swap(s.scRect.x,s.scRect.x1);
  if(s.scRect.y>s.scRect.y1)
    std::swap(s.scRect.y,s.scRect.y1);
  }

void Painter::setScissor(int x, int y, unsigned w, unsigned h) {
  s.scRect.x  = x;
  s.scRect.y  = y;
  s.scRect.x1 = x+int(w);
  s.scRect.y1 = y+int(h);
  }

void Painter::setScissor(const Rect& r) {
  setScissor(r.x,r.y,r.w,r.h);
  }

void Painter::implAddPoint(float x, float y, float u, float v) {
  pt.x=x*s.tr.invW-1.f;
  pt.y=y*s.tr.invH-1.f;
  pt.u=u;
  pt.v=v;
  dev.addPoint(pt);
  }

void Painter::implAddPoint(int x, int y, float u, float v) {
  pt.x=x*s.tr.invW-1.f;
  pt.y=y*s.tr.invH-1.f;
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
  implDrawTrig( float(x0), float(y0), s.dU+u0*s.invW,s.dV+v0*s.invH,
                float(x1), float(y1), s.dU+u1*s.invW,s.dV+v1*s.invH,
                float(x2), float(y2), s.dU+u2*s.invW,s.dV+v2*s.invH,
                trigBuf, 0 );
  }

void Painter::drawTriangle(float x0, float y0, float u0, float v0,
                           float x1, float y1, float u1, float v1,
                           float x2, float y2, float u2, float v2) {
  FPoint trigBuf[4+4+4+4];
  implDrawTrig( x0, y0, s.dU+u0*s.invW,s.dV+v0*s.invH,
                x1, y1, s.dU+u1*s.invW,s.dV+v1*s.invH,
                x2, y2, s.dU+u2*s.invW,s.dV+v2*s.invH,
                trigBuf, 0 );
  }

void Painter::implDrawTrig( float x0, float y0, float u0, float v0,
                            float x1, float y1, float u1, float v1,
                            float x2, float y2, float u2, float v2,
                            FPoint* out,
                            int stage ) {
  ScissorRect& sc = s.scRect;
  FPoint*      r  = out;

  float x[4] = {x0,x1,x2,x0};
  float y[4] = {y0,y1,y2,y0};
  float u[4] = {u0,u1,u2,u0};
  float v[4] = {v0,v1,v2,v0};

  float mid;
  float sx = 0.f;
  bool  cs = false, ns = false;

  switch( stage ) {
    case 0: sx = float(sc.x);  break;
    case 1: sx = float(sc.y);  break;
    case 2: sx = float(sc.x1); break;
    case 3: sx = float(sc.y1); break;
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
      implDrawTrig( r[  0].x, r[  0].y, r[  0].u, r[  0].v,
                    r[i+1].x, r[i+1].y, r[i+1].u, r[i+1].v,
                    r[i+2].x, r[i+2].y, r[i+2].u, r[i+2].v,
                    out, stage+1 );
      }
    } else {
    for(ptrdiff_t i=0;i<=count;++i){
      implAddPoint(r[  0].x, r[  0].y, r[  0].u, r[  0].v);
      implAddPoint(r[i+1].x, r[i+1].y, r[i+1].u, r[i+1].v);
      implAddPoint(r[i+2].x, r[i+2].y, r[i+2].u, r[i+2].v);
      }
    }
  }

void Painter::setBrush(const Brush& b) {
  if(state==StBrush)
    implBrush(b);

  s.br   = b;
  s.invW = b.info.invW;
  s.invH = b.info.invH;
  s.dU   = b.info.dx;
  s.dV   = b.info.dy;
  }

void Painter::setPen(const Pen &p) {
  if(state==StPen)
    implPen(p);
  s.pn = p;
  }

void Painter::implBrush(const Brush &b) {
  if(b.tex) {
    dev.setState(b.tex,b.color,b.texFrm,b.clamp);
    } else {
    dev.setState(b.spr,b.color);
    }
  dev.setBlend(b.blend);
  implSetColor(b.color.r(),b.color.g(),b.color.b(),b.color.a());
  }

void Painter::implPen(const Pen &p) {
  dev.setState(Brush::TexPtr(),p.color,TextureFormat::Undefined,ClampMode::Repeat);
  dev.setBlend(Blend::NoBlend);
  implSetColor(p.color.r(),p.color.g(),p.color.b(),p.color.a());
  }

void Painter::implDrawRect(int x1, int y1, int x2, int y2, float u1, float v1, float u2, float v2) {
  if(state!=StBrush) {
    dev.setTopology(Triangles);
    state=StBrush;
    implBrush(s.br);
    }

  if(T_LIKELY(s.tr.mat.type()==Transform::T_AxisAligned)) {
    s.tr.mat.map(x1,y1, x1,y1);
    s.tr.mat.map(x2,y2, x2,y2);
    if(T_UNLIKELY(x1>=x2)) {
      if(T_UNLIKELY(x1==x2))
        return;
      std::swap(x1,x2);
      std::swap(u1,u2);
      }
    if(T_UNLIKELY(y1>=y2)) {
      if(T_UNLIKELY(y1==y2))
        return;
      std::swap(y1,y2);
      std::swap(v1,v2);
      }

    float invW = (u2-u1)/float(x2-x1);
    float invH = (v2-v1)/float(y2-y1);

    ScissorRect& sc = s.scRect;
    if(x1<sc.x){
      int dx=sc.x-x1;
      x1+=dx;
      if(x1>=x2)
        return;
      u1+=dx*invW;
      }

    if(sc.x1<x2){
      int dx=sc.x1-x2;
      x2+=dx;
      if(x1>=x2)
        return;
      u2+=dx*invW;
      }

    if(y1<sc.y){
      int dy=sc.y-y1;
      y1+=dy;
      if(y1>=y2)
        return;
      v1+=dy*invH;
      }

    if(sc.y1<y2){
      int dy=sc.y1-y2;
      y2+=dy;
      if(y1>=y2)
        return;
      v2+=dy*invH;
      }

    implAddPoint(x1,y1, u1,v1);
    implAddPoint(x2,y1, u2,v1);
    implAddPoint(x2,y2, u2,v2);

    implAddPoint(x1,y1, u1,v1);
    implAddPoint(x2,y2, u2,v2);
    implAddPoint(x1,y2, u1,v2);
    } else {
    float x[4] = {float(x1), float(x2), float(x2), float(x1)};
    float y[4] = {float(y1), float(y1), float(y2), float(y2)};
    for(size_t i=0;i<4;++i)
      s.tr.mat.map(x[i],y[i],x[i],y[i]);

    FPoint trigBuf[4+4+4+4];
    implDrawTrig( x[0], y[0], u1, v1,
                  x[1], y[1], u2, v1,
                  x[2], y[2], u2, v2,
                  trigBuf, 0 );
    implDrawTrig(x[0],y[0], u1,v1,
                 x[2],y[2], u2,v2,
                 x[3],y[3], u1,v2,
                 trigBuf, 0 );
    }
  }

void Painter::drawRect(int x, int y, int w, int h, float u1, float v1, float u2, float v2) {
  implDrawRect(x,y,x+w,y+h, s.dU+u1*s.invW,s.dV+v1*s.invH,u2*s.invW+s.dU,v2*s.invH+s.dV);
  }

void Painter::drawRect(int x, int y, int w, int h, int u1, int v1, int u2, int v2) {
  implDrawRect(x,y,x+w,y+h, s.dU+u1*s.invW,s.dV+v1*s.invH,u2*s.invW+s.dU,v2*s.invH+s.dV);
  }

void Painter::drawRect(int x, int y, int w, int h) {
  implDrawRect(x,y,x+w,y+h, s.dU,s.dV,w*s.invW+s.dU,h*s.invH+s.dV);
  }

void Painter::drawRect(int x, int y, unsigned w, unsigned h) {
  implDrawRect(x,y,x+int(w),y+int(h), s.dU,s.dV,w*s.invW+s.dU,h*s.invH+s.dV);
  }

void Painter::drawRect(const Rect& rect) {
  drawRect(rect.x,rect.y,rect.w,rect.h);
  }

void Painter::drawLine(int ix1, int iy1, int ix2, int iy2) {
  ScissorRect& sc = s.scRect;
  if( sc.x > sc.x1 || sc.y > sc.y1 ) {
    // scissor algo cannot work with empty rect
    return;
    }
  /*
  if(penWidth>1.f){
    float dx=-(y2-y1),dy=x2-x1;
    return;
    }*/

  float x1,y1,x2,y2;

  s.tr.mat.map(float(ix1),float(iy1),x1,y1);
  s.tr.mat.map(float(ix2),float(iy2),x2,y2);

  if( x2<x1 ){
    std::swap(x2, x1);
    std::swap(y2, y1);
    }

  if(x1>sc.x1 && x2>sc.x1)
    return;
  if(x1<sc.x  && x2<sc.x)
    return;
  if(y1>sc.y1 && y2>sc.y1)
    return;
  if(y1<sc.y  && y2<sc.y)
    return;

  if( x1<sc.x ){
    if(std::fabs(x1-x2)<0.0001f)
      return;
    y1 += (sc.x-x1)*(y2-y1)/(x2-x1);
    x1 = float(sc.x);
    }

  if( x2 > sc.x1 ){
    if(std::fabs(x1-x2)<0.0001f)
      return;
    y2 += (sc.x1-x2)*(y2-y1)/(x2-x1);
    x2 = float(sc.x1);
    }

  if( y2<y1 ){
    std::swap(x2, x1);
    std::swap(y2, y1);
    }

  if( y1<sc.y ){
    if(std::fabs(y1-y2)<0.0001f)
      return;
    x1 += (sc.y-y1)*(x2-x1)/(y2-y1);
    y1 = float(sc.y);
    }

  if( y2 > sc.y1 ){
    if(std::fabs(y1-y2)<0.0001f)
      return;
    x2 += (sc.y1-y2)*(x2-x1)/(y2-y1);
    y2 = float(sc.y1);
    }

  if(state!=StPen){
    dev.setTopology(Lines);
    state=StPen;
    implPen(s.pn);
    }
  implAddPoint(x1+0.5f,y1+0.5f, 0,0);
  implAddPoint(x2+0.5f,y2+0.5f, 0,0);
  }

void Painter::drawLine(const Point& a, const Point& b) {
  drawLine(a.x,a.y,b.x,b.y);
  }

void Painter::setFont(const Font &f) {
  s.fnt=f;
  }

void Painter::translate(const Point& p) {
  s.tr.mat.translate(p);
  }

void Painter::translate(int x, int y) {
  s.tr.mat.translate(float(x),float(y));
  }

void Painter::rotate(float angle) {
  s.tr.mat.rotate(angle);
  }

void Painter::pushState() {
  stStk.push_back(s);
  }

void Painter::popState() {
  s = std::move(stStk.back());
  stStk.pop_back();
  switch(state) {
    case StNo: break;
    case StBrush: implBrush(s.br); break;
    case StPen:   implPen  (s.pn); break;
    }
  }

void Painter::drawText(int x, int y, const char *txt) {
  if(txt==nullptr)
    return;
  auto pb=s.br;
  Utf8Iterator i(txt);
  while(i.hasData()) {
    auto c=i.next();
    if(c=='\0'){
      setBrush(pb);
      return;
      }
    auto l=s.fnt.letter(c,ta);

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
  auto pb=s.br;

  for(;*txt;++txt) {
    auto& l=s.fnt.letter(*txt,ta);

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

static Utf8Iterator advanceByWord(Utf8Iterator i, int& x, int w, const Font& fnt, TextureAtlas& ta){
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

static Utf8Iterator advanceByLine(Utf8Iterator i, int& x, int w, const Font& fnt, TextureAtlas& ta){
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

static int calcLineWidth(Utf8Iterator i, Utf8Iterator eol, const Font& fnt, TextureAtlas& ta) {
  int x = 0;
  while(i!=eol){
    auto c=i.next();
    if(c=='\0')
      return x;
    if(c=='\n' || c=='\r')
      continue;
    auto l=fnt.letter(c,ta);
    x += l.advance.x;
    }
  return x;
  }

static int calcTextHeight(const char* txt, const int w, int& l0, const Font& fnt, TextureAtlas& ta) {
  int x = 0,  y=0, cnt = 0;

  Utf8Iterator i(txt);
  while(i.hasData()) {
    Utf8Iterator eol = advanceByLine(i,x,w,fnt,ta);
    if(i==eol)
      break;
    cnt++;
    x = 0;
    if(cnt>1) {
      i = eol;
      continue;
      }
    while(i!=eol){
      auto c=i.next();
      if(c=='\0'){
        return y;
        }
      if(c=='\n' || c=='\r') {
        continue;
        }
      auto l=fnt.letter(c,ta);
      y = std::max(l.size.h+l.dpos.y,y);
      }
    }

  l0 = y;
  if(cnt>1)
    y += (cnt-1)*int(std::ceil(fnt.pixelSize()));
  return y;
  }

void Painter::drawText(int rx, int ry, int w, int h, const char *txt, AlignFlag flg) {
  if(txt==nullptr)
    return;
  auto pb=s.br;
  int  x = 0, y = 0, pSz = int(std::ceil(s.fnt.pixelSize()));

  if(flg!=0) {
    int l0=0;
    const int th = calcTextHeight(txt,w,l0,s.fnt,ta);
    if(flg & AlignVCenter)
      y = l0+(h-th)/2;
    else if(flg & AlignBottom)
      y = l0+(h-th);
    }

  Utf8Iterator i(txt);
  while(i.hasData()) {
    // make next line
    x = 0;
    Utf8Iterator eol = advanceByLine(i,x,w,s.fnt,ta);
    if(i==eol)
      break;

    x = 0;
    if(flg & AlignHCenter) {
      int wl = calcLineWidth(i,eol,s.fnt,ta);
      x += (w-wl)/2;
      }
    else if(flg & AlignRight) {
      int wl = calcLineWidth(i,eol,s.fnt,ta);
      x += (w-wl);
      }

    while(i!=eol) {
      auto c=i.next();
      if(c=='\0'){
        setBrush(pb);
        return;
        }
      if(c=='\n' || c=='\r')
        continue;
      auto l=s.fnt.letter(c,ta);

      if(!l.view.isEmpty()) {
        setBrush(Brush(l.view,pb.color,PaintDevice::Alpha));
        drawRect(rx+x+l.dpos.x,
                 ry+y+l.dpos.y,
                 l.view.w(),l.view.h());
        }

      x += l.advance.x;
      }
    y+=pSz;
    }
  setBrush(pb);
  }

void Painter::drawText(int x, int y, int w, int h, const std::string &txt, AlignFlag flg) {
  drawText(x,y,w,h,txt.c_str(),flg);
  }

void Painter::drawText(const Rect& r, const char* txt, AlignFlag flg) {
  drawText(r.x,r.y,r.w,r.h,txt,flg);
  }

void Painter::drawText(const Rect& r, const std::string& txt, AlignFlag flg) {
  drawText(r.x,r.y,r.w,r.h,txt,flg);
  }
