#include "textedit.h"

#include <Tempest/Painter>
#include <Tempest/Application>

using namespace Tempest;

TextEdit::TextEdit() {
  anim.timeout.bind(this,&Widget::update);
  anim.start(500);
  }

void TextEdit::invalidateSizeHint() {
  Size sz=textM.sizeHint();
  setSizeHint(sz,margins());
  }

void TextEdit::setText(const char *text) {
  textM.setText(text);
  invalidateSizeHint();
  update();
  }

void TextEdit::setFont(const Font &f) {
  textM.setFont(f);
  invalidateSizeHint();
  update();
  }

void TextEdit::mouseDownEvent(MouseEvent &e) {
  selS = textM.charAt(e.pos());
  selE = selS;
  update();
  }

void TextEdit::mouseDragEvent(MouseEvent &e) {
  selE = textM.charAt(e.pos());
  update();
  }

void TextEdit::paintEvent(PaintEvent &e) {
  Painter p(e);
  p.setBrush(Color(0,0,0,0.2f));
  p.drawRect(0,0,textM.w(),textM.h());

  if(selS==selE) {
    if((Application::tickCount()/500)%2)
      textM.drawCursor(p,0,0,selS);
    } else {
    textM.drawCursor(p,0,0,selS,selE);
    }

  textM.paint(p,0,int(textM.font().pixelSize()));
  }
