#include "texteditor.h"

#include <Tempest/TextEdit>

#include "resources.h"

using namespace Tempest;

TextEditor::TextEditor(){
  const Tempest::Font& font = Resources::get<Tempest::Font>("font/Roboto.ttf");
  setLayout(Vertical);
  TextEdit& ln = addWidget(new TextEdit());
  ln.setFont(font);
  ln.setText("Caption\nLine 1\nLine 2");
  }
