#include "button.h"

#include <Tempest/Painter>
#include <Tempest/Brush>
#include <Tempest/Menu>
#include <Tempest/Application>

using namespace Tempest;

Button::Button() {
  textM.setFont(Application::font());
  setMargins(4);
  auto& m = style().metrics();
  setSizeHint(Size(m.buttonSize,m.buttonSize));
  setSizePolicy(Preferred,Fixed);
  setFocusPolicy(ClickFocus);
  }

Button::~Button() {
  }

void Button::polishEvent(PolishEvent&) {
  invalidateSizeHint();
  }

void Button::emitClick() {
  onClick();
  }

void Button::updateFont() {
  const bool usr = !fnt.isEmpty();
  if(usr==fntInUse && !usr)
    return;

  fntInUse = usr;
  if(fntInUse)
    textM.setFont(fnt); else
    textM.setFont(Application::font());
  }

void Button::setText(const char *text) {
  textM.setText(text);
  invalidateSizeHint();
  update();
  }

void Button::setText(const std::string& text) {
  textM.setText(text.c_str());
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

void Button::setButtonType(Button::Type t) {
  auto st = state();
  st.button = t;
  setWidgetState(st);
  }

Button::Type Button::buttonType() const {
  return state().button;
  }

void Button::mouseDownEvent(MouseEvent &e) {
  if(!isEnabled())
    return;
  setPressed(e.button==Event::ButtonLeft);
  }

void Button::mouseUpEvent(MouseEvent& e) {
  if( e.x<=w() && e.y<=h() &&  e.x>=0 && e.y>=0 && isEnabled() ) {
    if(buttonType()==T_CheckableButton) {
      auto c = isChecked();
      setChecked(c==Checked ? Unchecked : Checked);
      }

    if(isPressed())
      emitClick();
    else if(e.button==Event::ButtonRight)
      showMenu();
    }
  setPressed(false);
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
  updateFont();
  auto& m  = style().metrics();
  auto  sz = style().sizeHint(this,Style::E_Background,&textM,Style::Extra(*this));
  sz.w = std::max(sz.w,m.buttonSize);
  sz.h = m.buttonSize;
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

void Button::setChecked(bool c) {
  setChecked(c ? Checked : Unchecked);
  }

void Button::setChecked(WidgetState::CheckState c) {
  if(state().checked==c)
    return;
  auto st=state();
  st.checked=c;
  setWidgetState(st);
  update();
  }

Button::CheckState Button::isChecked() const {
  return state().checked;
  }

bool Button::isPressed() const {
  return state().pressed;
  }

void Button::setMenu(Menu* menu) {
  btnMenu.reset(menu);
  }

const Menu* Button::menu() const {
  return btnMenu.get();
  }

void Button::showMenu() {
  if(btnMenu!=nullptr) {
    bool p = isPressed();
    setPressed(true);
    btnMenu->exec(*this);
    setPressed(p);
    }
  }
