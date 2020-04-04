#include "scrollbar.h"

#include <Tempest/Layout>
#include <Tempest/Application>
#include <Tempest/Painter>

using namespace Tempest;

ScrollBar::ScrollBar(Tempest::Orientation ori) : orient(ori) {
  mSmallStep = 10;
  mLargeStep = 20;
  implSetOrientation(orient);
  timer.timeout.bind(this,&ScrollBar::processPress);

  auto& m = style().metrics();
  setSizeHint(Size(m.scrollbarSize,m.scrollbarSize));
  }

void ScrollBar::polishEvent(PolishEvent&) {
  auto& m = style().metrics();
  setSizeHint(Size(m.scrollbarSize,m.scrollbarSize));
  }

void ScrollBar::setOrientation(Tempest::Orientation ori) {
  if(orient==ori)
    return;
  implSetOrientation(ori);
  }

void ScrollBar::implSetOrientation(Orientation ori) {
  setLayout( ori );

  Tempest::SizePolicy p = sizePolicy();
  if( ori==Tempest::Vertical ){
    p.typeH = Tempest::Fixed;
    p.typeV = Tempest::Preferred;
    } else {
    p.typeH = Tempest::Preferred;
    p.typeV = Tempest::Fixed;
    }
  setSizePolicy(p);

  orient = ori;
  onOrientationChanged(ori);
  update();
  }

WidgetState& ScrollBar::stateOf(WidgetState& out, ScrollBar::Elements e) const {
  out = state();
  if((pressed&e)==e)
    out.pressed = true;
  return out;
  }

Tempest::Orientation ScrollBar::orientation() const {
  return orient;
  }

void ScrollBar::setRange(int min, int max) {
  if( min>max )
    std::swap(min, max);

  rmin = min;
  rmax = max;
  int nmvalue = std::max( rmin, std::min(mValue, rmax) );

  mSmallStep = std::max<int>(1, int(std::min<int64_t>( 10, range()/100 )));
  mLargeStep = std::max<int>(1, int(std::min<int64_t>( 20, range()/10 )));

  setValue( nmvalue );
  }

int64_t ScrollBar::range() const {
  return rmax-rmin;
  }

int ScrollBar::minValue() const {
  return rmin;
  }

int ScrollBar::maxValue() const {
  return rmax;
  }

void ScrollBar::setValue(int v) {
  v = std::max( rmin, std::min(v, rmax) );

  if( v!=mValue ){
    mValue = v;
    onValueChanged(v);
    update();
    }
  }

void ScrollBar::setSmallStep(int step) {
  mSmallStep = step>=0 ? step : 0;
  }

void ScrollBar::setLargeStep(int step) {
  mLargeStep = step>=0 ? step : 0;
  }

void ScrollBar::setCentralButtonSize(int sz) {
  auto& m = style().metrics();
  cenBtnSize = std::max<int>(m.scrollButtonSize, sz);
  cenBtnSize = sz;
  }

int ScrollBar::centralAreaSize() const {
  auto r = elementRect(Elt_Space);
  if(orientation()==Vertical)
    return r.h; else
    return r.w;
  }

void ScrollBar::hideArrowButtons() {
  elements = Elements(elements|Elt_Inc|Elt_Dec);
  update();
  }

void ScrollBar::showArrawButtons() {
  elements = Elements(elements & ~(Elt_Inc|Elt_Dec));
  update();
  }

void ScrollBar::resizeEvent(Tempest::SizeEvent &) {
  update();
  }

void ScrollBar::mouseDownEvent(MouseEvent& e) {
  mpos    = elementRect(Elt_Cen).pos()-e.pos();
  pressed = tracePoint(e.pos());
  processPress();
  update();
  timer.start(50);
  }

void ScrollBar::mouseDragEvent(MouseEvent& e) {
  if(pressed!=Elt_Cen)
    return;

  auto cen = elementRect(Elt_Cen);
  auto sp  = elementRect(Elt_Space);
  if(orientation()==Vertical) {
    if(sp.h==cen.h)
      return;

    cen.y = mpos.y+e.y;
    if(cen.y<sp.y)
      cen.y = sp.y;
    if(cen.y+cen.h>sp.y+sp.h)
      cen.y = sp.y+sp.h-cen.h;
    float k = float(cen.y-sp.y)/float(sp.h-cen.h);
    setValue(rmin+int((rmax-rmin)*k));
    } else {
    if(sp.w==cen.w)
      return;

    cen.x = mpos.x+e.x;
    if(cen.x<sp.x)
      cen.x = sp.x;
    if(cen.x+cen.w>sp.x+sp.w)
      cen.x = sp.x+sp.w-cen.w;
    float k = float(cen.x-sp.x)/float(sp.w-cen.w);
    setValue(rmin+int((rmax-rmin)*k));
    }
  }

void ScrollBar::mouseUpEvent(MouseEvent&) {
  pressed = Elt_None;
  timer.stop();
  update();
  }

void ScrollBar::mouseWheelEvent(MouseEvent &e) {
  if(!isEnabled())
    return;

  if(e.delta>0)
    decL(); else
  if(e.delta<0)
    incL();
  }

void ScrollBar::processPress() {
  if(pressed==Elt_Inc)
    inc();
  if(pressed==Elt_Dec)
    dec();
  if(pressed==Elt_IncL)
    incL();
  if(pressed==Elt_DecL)
    decL();
  }

void ScrollBar::inc() {
  setValue( value()+mSmallStep );
  }

void ScrollBar::dec() {
  setValue( value()-mSmallStep );
  }

void ScrollBar::incL() {
  setValue( value()+mLargeStep );
  }

void ScrollBar::decL() {
  setValue( value()-mLargeStep );
  }

Rect ScrollBar::elementRect(ScrollBar::Elements e) const {
  const int wx    = (orient==Orientation::Vertical) ? w() : h();
  const int sz    = std::max((orient==Orientation::Vertical) ? h() : w(), wx*2);

  switch(e) {
    case Elt_None:
      return Rect();
    case Elt_Dec: {
      if((elements&Elt_Dec)==0)
        return Rect();
      return Rect(0,0,wx,wx);
      }
    case Elt_Inc: {
      if((elements&Elt_Inc)==0)
        return Rect();
      if(orient==Orientation::Vertical)
        return Rect(0,sz-wx,wx,wx); else
        return Rect(sz-wx,0,wx,wx);
      }
    case Elt_Space: {
      int x     = 0;
      int space = sz;
      if((elements&Elt_Cen)==0)
        return Rect();
      if((elements&Elt_Inc)!=0) {
        space -= wx;
        x+=wx;
        }
      if((elements&Elt_Dec)!=0)
        space -= wx;

      if(orient==Orientation::Vertical)
        return Rect(0,x,w(),space); else
        return Rect(x,0,space,h());
      }
    case Elt_Cen: {
      int space = sz;
      if((elements&Elt_Cen)==0)
        return Rect();
      if((elements&Elt_Inc)!=0)
        space -= wx;
      if((elements&Elt_Dec)!=0)
        space -= wx;

      int cen   = std::min(cenBtnSize,space);
      float k = mValue/float(rmax-rmin);
      int   x = int(float(space-cen)*k);

      if(orient==Orientation::Vertical)
        return Rect(0,wx+x,w(),cen); else
        return Rect(wx+x,0,cen,h());
      }
    case Elt_IncL:
    case Elt_DecL:
      return Rect();
    }
  return Rect();
  }

ScrollBar::Elements ScrollBar::tracePoint(const Point& p) const {
  if(elementRect(Elt_Inc).contains(p,true))
    return Elt_Inc;
  if(elementRect(Elt_Dec).contains(p,true))
    return Elt_Dec;
  auto cr = elementRect(Elt_Cen);
  if(cr.contains(p,true))
    return Elt_Cen;
  if(orient==Orientation::Vertical) {
    if(p.y<cr.y)
      return Elt_DecL;
    if(p.y>cr.y+cr.h)
      return Elt_IncL;
    } else {
    if(p.x<cr.x)
      return Elt_DecL;
    if(p.x>cr.x+cr.y)
      return Elt_IncL;
    }
  return Elt_None;
  }

void ScrollBar::paintEvent(PaintEvent &e) {
  WidgetState tmpSt;
  {
    Painter p(e);
    style().draw(p,this,Style::E_Background,state(),Rect(0,0,w(),h()),Style::Extra(*this));
  }

  if((elements&Elt_Inc)!=0) {
    auto& st = stateOf(tmpSt,Elt_Inc);
    Painter p(e);
    if( orientation()==Vertical )
      style().draw(p,this,Style::E_ArrowDown, st,elementRect(Elt_Inc),Style::Extra(*this)); else
      style().draw(p,this,Style::E_ArrowRight,st,elementRect(Elt_Inc),Style::Extra(*this));
    }

  if((elements&Elt_Dec)!=0) {
    auto& st = stateOf(tmpSt,Elt_Dec);
    Painter p(e);
    if( orientation()==Vertical )
      style().draw(p,this,Style::E_ArrowUp,  st,elementRect(Elt_Dec),Style::Extra(*this)); else
      style().draw(p,this,Style::E_ArrowLeft,st,elementRect(Elt_Dec),Style::Extra(*this));
    }

  if((elements&Elt_Cen)!=0) {
    auto& st = stateOf(tmpSt,Elt_Cen);
    Painter p(e);
    style().draw(p,this,Style::E_CentralButton,st,elementRect(Elt_Cen),Style::Extra(*this));
    }
  }
