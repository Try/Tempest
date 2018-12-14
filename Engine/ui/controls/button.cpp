#include "button.h"

#include <Tempest/Painter>
#include <Tempest/Brush>

using namespace Tempest;

Button::Button() {
  setMargins(8);
  setSizeHint(128,27);
  setSizePolicy(Preferred,Fixed);
  }

void Button::setText(const char *text) {
  textM.setText(text);
  setSizeHint(textM.sizeHint(),margins());

  //update();
  }

void Button::setFont(const Font &f) {
  textM.setFont(f);
  setSizeHint(textM.sizeHint(),margins());
  }

void Button::mouseDownEvent(MouseEvent &e) {
  if(e.button!=Event::ButtonLeft){
    e.ignore();
    return;
    }
  }

void Button::paintEvent(PaintEvent &e) {
  Painter p(e);

  p.setBrush(Color(0,0,0,0.2f));
  p.drawRect(0,0,w(),h());

  auto s=textM.wrapSize();
  textM.paint(p,(w()-s.w)/2,int(h()+s.h)/2);
  }
