#include "label.h"

#include <Tempest/Painter>

using namespace Tempest;

Label::Label() {
  setSizeHint(27,27);
  setSizePolicy(Preferred,Fixed);
  setFocusPolicy(ClickFocus);
  }

void Label::setFont(const Font &f) {
  textM.setFont(f);
  invalidateSizeHint();
  }

const Font &Label::font() const {
  return textM.font();
  }

void Label::setTextColor(const Color& c) {
  textCl = c;
  update();
  }

const Color& Label::textColor() const {
  return textCl;
  }

void Label::setText(const char* t) {
  textM.setText(t);
  invalidateSizeHint();
  update();
  }

void Label::setText( const std::string &t ) {
  textM.setText(t.c_str());
  invalidateSizeHint();
  update();
  }

void Label::paintEvent(PaintEvent &e) {
  Painter p(e);
  style().draw(p,this, Style::E_Background, state(),Rect(0,0,w(),h()),Style::Extra(*this));
  style().draw(p,textM,Style::TE_LabelTitle,state(),Rect(0,0,w(),h()),Style::Extra(*this));
  }

void Label::invalidateSizeHint() {
  auto sz = style().sizeHint(this,Style::E_Background,&textM,Style::Extra(*this));
  setSizeHint(sz,margins());
  }
