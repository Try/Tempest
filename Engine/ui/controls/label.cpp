#include "label.h"

#include <Tempest/Painter>
#include <Tempest/Application>

using namespace Tempest;

Label::Label() {
  textM.setFont(Application::font());
  setMargins(4);
  setSizePolicy(Preferred,Fixed);
  setFocusPolicy(ClickFocus);
  invalidateSizeHint();
  }

void Label::updateFont() {
  const bool usr = !fnt.isEmpty();
  if(usr==fntInUse && !usr)
    return;

  fntInUse = usr;
  if(fntInUse)
    textM.setFont(fnt); else
    textM.setFont(Application::font());
  }

void Label::setFont(const Font &f) {
  fnt = f;
  updateFont();
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
  updateFont();
  textM.setText(t);
  invalidateSizeHint();
  update();
  }

void Label::setText(const std::string &t) {
  updateFont();
  textM.setText(t.c_str());
  invalidateSizeHint();
  update();
  }

void Label::invalidateSizeHint() {
  auto sz = style().sizeHint(this,Style::E_Background,&textM,Style::Extra(*this));
  setSizeHint(sz,margins());
  }

void Label::paintEvent(PaintEvent &e) {
  Painter p(e);
  style().draw(p,this, Style::E_Background, state(),Rect(0,0,w(),h()),Style::Extra(*this));
  style().draw(p,textM,Style::TE_LabelTitle,state(),Rect(0,0,w(),h()),Style::Extra(*this));
  }
