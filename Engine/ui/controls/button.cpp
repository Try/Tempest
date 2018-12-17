#include "button.h"

#include <Tempest/Painter>
#include <Tempest/Brush>

using namespace Tempest;

Button::Button() {
  setMargins(8);
  setSizeHint(27,27);
  setSizePolicy(Preferred,Fixed);
  }

void Button::setText(const char *text) {
  textM.setText(text);
  invalidateSizeHint();

  update();
  }

void Button::setFont(const Font &f) {
  textM.setFont(f);
  invalidateSizeHint();

  update();
  }

void Button::setIcon(const Sprite &s) {
  icon=s;
  invalidateSizeHint();

  update();
  }

void Button::mouseDownEvent(MouseEvent &e) {
  if(e.button!=Event::ButtonLeft){
    e.ignore();
    return;
    }
  }

void Button::mouseUpEvent(MouseEvent &e) {
  e.accept();
  }

void Button::paintEvent(PaintEvent &e) {
  Painter p(e);

  p.setBrush(Color(0,0,0,0.2f));
  p.drawRect(0,0,w(),h());

  auto& m = margins();
  int dx=0;
  if(!icon.isEmpty()) {
    p.setBrush(icon);
    p.drawRect(m.left,(h()-int(icon.h()))/2,icon.w(),icon.h());
    dx = dx+int(icon.w())+spacing();
    }

  auto ts=textM.wrapSize();
  //auto is=icon.size();

  textM.paint(p,m.left+dx+(w()-ts.w-dx-margins().xMargin())/2,int(h()+ts.h)/2);
  }

void Button::invalidateSizeHint() {
  Size sz=textM.sizeHint();
  Size is=icon.size();

  if(is.w>0)
    sz.w = sz.w+is.w+spacing();
  sz.h = std::max(sz.h,is.h);

  setSizeHint(sz,margins());
  }
