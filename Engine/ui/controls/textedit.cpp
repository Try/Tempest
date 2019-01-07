#include "textedit.h"

#include <Tempest/Painter>

using namespace Tempest;

TextEdit::TextEdit() {
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
  cursor = textM.charAt(e.pos());
  }

void TextEdit::paintEvent(PaintEvent &e) {
  Painter p(e);
  p.setBrush(Color(0,0,0,0.2f));
  p.drawRect(0,0,textM.w(),textM.h());

  textM.paint(p,0,int(textM.font().pixelSize()));
  }
