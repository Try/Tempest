#include "button.h"

#include <Tempest/Painter>
#include <Tempest/Brush>

using namespace Tempest;

Button::Button() {
  setMargins(4);
  setSizeHint(27,27);
  setSizePolicy(Preferred,Fixed);
  setFocusPolicy(ClickFocus);
  }

void Button::setText(const char *text) {
  textM.setText(text);
  invalidateSizeHint();

  update();
  }

void Button::setFont(const Font &f) {
  textM.setFont(f);
  invalidateSizeHint();

  update();
  }

const Font& Button::font() const {
  return textM.font();
  }

void Button::setIcon(const Icon& s) {
  icn=s;
  invalidateSizeHint();

  update();
  }

void Button::mouseDownEvent(MouseEvent &e) {
  if(e.button!=Event::ButtonLeft){
    e.ignore();
    return;
    }
  if(!isEnabled())
    return;
  setPressed(e.button==Event::ButtonLeft);
  update();
  }

void Button::mouseUpEvent(MouseEvent& e) {
  if( e.x<=w() && e.y<=h() &&  e.x>=0 && e.y>=0 && isEnabled() ){
    if(isPressed())
      onClick();
    else if(e.button==Event::ButtonRight)
      showMenu();
    }
  setPressed(false);
  update();
  }

void Button::mouseMoveEvent(MouseEvent &e) {
  e.accept();
  }

void Button::mouseEnterEvent(MouseEvent&) {
  update();
  }

void Button::mouseLeaveEvent(MouseEvent&) {
  update();
  }

void Button::paintEvent(PaintEvent &e) {
  Tempest::Painter p(e);
  style().draw(p,this, Style::E_Background,  state(),Rect(0,0,w(),h()),Style::Extra(*this));
  style().draw(p,textM,Style::TE_ButtonTitle,state(),Rect(0,0,w(),h()),Style::Extra(*this));
  }

void Button::invalidateSizeHint() {
  auto& icon = icn.sprite(w(),h(),Icon::ST_Normal);
  Size sz=textM.sizeHint();
  Size is=icon.size();

  if(is.w>0)
    sz.w = sz.w+is.w+spacing();
  sz.h = std::max(sz.h,is.h);

  setSizeHint(sz,margins());
  }

void Button::setPressed(bool p) {
  if(state().pressed==p)
    return;
  auto st=state();
  st.pressed=p;
  setWidgetState(st);
  update();
  }

void Button::showMenu() {
  // TODO
  }

bool Button::isPressed() const {
  return state().pressed;
  }
