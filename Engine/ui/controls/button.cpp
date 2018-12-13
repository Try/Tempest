#include "button.h"

#include <Tempest/Painter>
#include <Tempest/Brush>

using namespace Tempest;

Button::Button() {
  setSizeHint(128,27);
  setSizePolicy(Preferred,Fixed);
  }

void Button::setText(const char *text) {
  textM=TextModel(text);
  setSizeHint(textM.sizeHint());
  }

void Button::setFont(const Font &f) {
  textM.setFont(f);
  setSizeHint(textM.sizeHint());
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
  }
