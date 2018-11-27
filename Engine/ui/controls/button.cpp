#include "button.h"

#include <Tempest/Painter>

using namespace Tempest;

Button::Button() {
  setSizeHint(128,27);
  setSizePolicy(Preferred,Fixed);
  }

void Button::mouseDownEvent(MouseEvent &e) {
  if(e.button!=Event::ButtonLeft){
    e.ignore();
    return;
    }
  }

void Button::paintEvent(PaintEvent &e) {
  Painter p(e);

  p.setBrush(Color(0,0,0,0.8f));
  p.drawRect(0,0,w(),h());
  }
