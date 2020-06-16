#include "textedit.h"

using namespace Tempest;

TextEdit::TextEdit() {
  setMargins(Margin(4,4,0,0));
  }

void TextEdit::setText(const char *text) {
  AbstractTextInput::setText(text);
  }
