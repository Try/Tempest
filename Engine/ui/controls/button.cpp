#include "button.h"

#include <Tempest/Painter>
#include <Tempest/Brush>

using namespace Tempest;

Button::Button() {
  setMargins(4);
  setSizeHint(27,27);
  setSizePolicy(Preferred,Fixed);
  setFocusPolicy(ClickFocus);
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
  //onClick();
  }

void Button::mouseUpEvent(MouseEvent&) {
  onClick();
  }

void Button::mouseMoveEvent(MouseEvent &e) {
  e.accept();
  }

void Button::mouseEnterEvent(MouseEvent&) {
  update();
  }

void Button::mouseLeaveEvent(MouseEvent&) {
  update();
  }

void Button::paintEvent(PaintEvent &e) {
  Painter p(e);

  p.setBrush(isMouseOver() ? Color(0,0,0,0.2f) : Color(0,0,0,0.02f));
  p.drawRect(0,0,w(),h());

  auto& m = margins();
  int dx=0;
  if(!icon.isEmpty()) {
    p.setBrush(icon);
    int x=m.left;
    if(textM.isEmpty())
      x=(w()-icon.w())/2;
    p.drawRect(x,(h()-icon.h())/2,icon.w(),icon.h());
    dx = dx+icon.w()+spacing();
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
