#include "lineedit.h"

#include <Tempest/Utf8Iterator>

using namespace Tempest;

LineEdit::LineEdit() {
  setMargins(Margin(4,4,4,4));
  invalidateSizeHint();
  }

void LineEdit::setText(const char *text) {
  Utf8Iterator i(text);
  while(i.hasData()) {
    char32_t ch = i.next();
    if(ch=='\n' || ch=='\r')
      return filterAndSetText(text);
    }
  AbstractTextInput::setText(text);
  }

void LineEdit::filterAndSetText(const char* text) {
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
  AbstractTextInput::setText(str.c_str());
  }

void LineEdit::keyDownEvent(KeyEvent& e) {
  if(e.key==Event::K_Return) {
    onEnter();
    return;
    }
  AbstractTextInput::keyDownEvent(e);
  }

void LineEdit::keyRepeatEvent(KeyEvent& e) {
  if(e.key==Event::K_Return) {
    onEnter();
    return;
    }
  AbstractTextInput::keyDownEvent(e);
  }
