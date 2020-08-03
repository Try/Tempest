#include "checkbox.h"

#include <Tempest/Painter>

using namespace Tempest;

CheckBox::CheckBox() {
  setButtonType(Button::T_CheckableButton);
  }

CheckBox::~CheckBox() {
  }

void CheckBox::paintEvent(PaintEvent& e) {
  Tempest::Painter p(e);
  style().draw(p,this,  Style::E_Background,    state(),Rect(0,0,w(),h()),Style::Extra(*this));
  style().draw(p,text(),Style::TE_CheckboxTitle,state(),Rect(0,0,w(),h()),Style::Extra(*this));
  }
