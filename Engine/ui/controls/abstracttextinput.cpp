#include "abstracttextinput.h"

#include <Tempest/Painter>
#include <Tempest/Application>
#include <Tempest/TextCodec>

using namespace Tempest;

AbstractTextInput::AbstractTextInput() {
  stk.setMaxDepth(StkDepth);

  textM.setFont(Application::font());
  setFocusPolicy(StrongFocus);

  anim.timeout.bind(this,&AbstractTextInput::onRepaintCursor);
  // anim.start(Style::cursorFlashTime);

  scUndo = Shortcut(*this,KeyEvent::M_Command,KeyEvent::K_Z);
  scUndo.onActivated.bind(this,&AbstractTextInput::undo);

  scRedo = Shortcut(*this,KeyEvent::M_Command,KeyEvent::K_Y);
  scRedo.onActivated.bind(this,&AbstractTextInput::redo);
  }

void AbstractTextInput::invalidateSizeHint() {
  updateFont();
  Size sz=textM.sizeHint();
  sz.w = 0;
  setSizeHint(sz,margins());
  }

void AbstractTextInput::updateFont() {
  const bool usr = !fnt.isEmpty();
  if(usr==fntInUse && !usr)
    return;

  fntInUse = usr;
  if(fntInUse)
    textM.setFont(fnt); else
    textM.setFont(Application::font());
  }

void AbstractTextInput::undo() {
  stk.undo(textM);
  adjustSelection();
  update();
  }

void AbstractTextInput::redo() {
  stk.redo(textM);
  adjustSelection();
  update();
  }

void AbstractTextInput::adjustSelection() {
  if(!textM.isValid(selS))
    selS = textM.clamp(selS);
  if(!textM.isValid(selE))
    selE = textM.clamp(selE);
  }

void AbstractTextInput::setText(const char *text) {
  textM.setText(text);
  adjustSelection();
  invalidateSizeHint();
  update();
  }

void AbstractTextInput::setText(const std::string& text) {
  setText(text.c_str());
  }

const TextModel& AbstractTextInput::text() const {
  return textM;
  }

void AbstractTextInput::setFont(const Font &f) {
  fnt = f;
  invalidateSizeHint();
  update();
  }

const Font& AbstractTextInput::font() const {
  return fnt;
  }

void AbstractTextInput::setTextColor(const Color& cl) {
  fntCl = cl;
  update();
  }

const Color& AbstractTextInput::textColor() const {
  return fntCl;
  }

TextModel::Cursor AbstractTextInput::selectionStart() const {
  return selS;
  }

TextModel::Cursor AbstractTextInput::selectionEnd() const {
  return selE;
  }

void AbstractTextInput::setUndoRedoEnabled(bool enable) {
  undoEnable = enable;
  if(enable)
    stk.setMaxDepth(0); else
    stk.setMaxDepth(StkDepth);
  scUndo.setEnable(enable);
  scRedo.setEnable(enable);
  }

bool AbstractTextInput::isUndoRedoEnabled() const {
  return undoEnable;
  }

void AbstractTextInput::mouseDownEvent(MouseEvent &e) {
  updateFont();
  auto& m = margins();
  selS = textM.charAt(e.pos()-Point(m.left,m.top));
  selE = selS;
  update();
  }

void AbstractTextInput::mouseDragEvent(MouseEvent &e) {
  updateFont();
  auto& m = margins();
  selE = textM.charAt(e.pos()-Point(m.left,m.top));
  update();
  }

void AbstractTextInput::mouseUpEvent(MouseEvent& e) {
  e.accept();
  }

void AbstractTextInput::keyDownEvent(KeyEvent& e) {
  keyEventImpl(e);
  }

void AbstractTextInput::keyRepeatEvent(KeyEvent& e) {
  keyEventImpl(e);
  }

void AbstractTextInput::keyUpEvent(KeyEvent&) {
  }

void AbstractTextInput::keyEventImpl(KeyEvent& e) {
  if(e.modifier & KeyEvent::M_Ctrl) {
    e.ignore();
    return;
    }

  if(e.key==Event::K_Left) {
    selS = textM.advance(selS,-1);
    }
  else if(e.key==Event::K_Right) {
    selS = textM.advance(selE, 1);
    }

  if(e.key==Event::K_Delete || e.key==Event::K_Back) {
    TextModel::Cursor end = selE;
    if(selS==selE)
      end = textM.advance(selS, e.key==Event::K_Delete ? 1 : -1);
    if(textM.isValid(end)) {
      stk.push(textM, new TextModel::CommandErase(selS,end));
      selS = end;
      }
    }
  else if(e.code!=0) {
    char t[3] = {};
    if(e.key==Event::K_Return) {
      t[0] = '\n';
      } else {
      TextCodec::toUtf8(e.code,t);
      }
    if(selS==selE) {
      stk.push(textM, new TextModel::CommandInsert(t,selS));
      selS = textM.advance(selS,1);
      } else {
      if(selE<selS)
        std::swap(selE,selS);
      stk.push(textM, new TextModel::CommandReplace(t,selS,selE));
      selS = textM.advance(selS,1);
      }
    }
  selE = selS;
  update();
  }

void AbstractTextInput::paintEvent(PaintEvent &e) {
  Painter p(e);
  style().draw(p, this,  Style::E_Background,       state(), Rect(0,0,w(),h()), Style::Extra(*this));
  style().draw(p, textM, Style::TE_TextEditContent, state(), Rect(0,0,w(),h()), Style::Extra(*this));
  if(cursorState)
    ;//style().draw(p, textM, Style::TE_Cursor,          state(), Rect(0,0,w(),h()), Style::Extra(*this));
  }

void AbstractTextInput::focusEvent(FocusEvent& e) {
  if(e.in)
    anim.start(Style::cursorFlashTime);
  }

void AbstractTextInput::onRepaintCursor() {
  if(!hasFocus() && !cursorState) {
    anim.stop();
    return;
    }
  cursorState = !cursorState;
  update();
  }
