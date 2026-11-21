#include "label.h"

#include <Tempest/Painter>
#include <Tempest/Application>
#include <Tempest/Utf8Iterator>

using namespace Tempest;

Label::Label() {
  textM.setFont(Application::font());
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

void Label::setText(std::string_view text) {
  Utf8Iterator i(text);
  while(i.hasData()) {
    char32_t ch = i.next();
    if(ch=='\n' || ch=='\r')
      return filterAndSetText(text);
    }
  updateFont();
  textM.setText(text);
  invalidateSizeHint();
  update();
  }

void Label::filterAndSetText(std::string_view text) {
  std::string str;

  Utf8Iterator i(text);
  while(i.hasData()) {
    size_t i0 = i.pos();
    char32_t ch = i.next();
    if(ch=='\n' || ch=='\r')
      continue;

    size_t i1 = i.pos();
    for(size_t r=i0;r<i1;++r)
      str.push_back(text[r]);
    }
  updateFont();
  textM.setText(text);
  invalidateSizeHint();
  update();
  }

const TextModel& Label::text() const {
  return textM;
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
