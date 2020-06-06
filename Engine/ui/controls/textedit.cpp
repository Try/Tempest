#include "textedit.h"

#include <Tempest/Painter>
#include <Tempest/Application>
#include <Tempest/TextCodec>

using namespace Tempest;

TextEdit::TextEdit() {
  textM.setFont(Application::font());
  setFocusPolicy(StrongFocus);

  anim.timeout.bind(this,&TextEdit::onRepaintCursor);
  anim.start(Style::cursorFlashTime);

  scUndo = Shortcut(*this,KeyEvent::M_Command,KeyEvent::K_Z);
  scUndo.onActivated.bind(this,&TextEdit::undo);

  scRedo = Shortcut(*this,KeyEvent::M_Command,KeyEvent::K_Y);
  scRedo.onActivated.bind(this,&TextEdit::redo);

  setMargins(4);
  }

void TextEdit::invalidateSizeHint() {
  updateFont();
  Size sz=textM.sizeHint();
  setSizeHint(sz,margins());
  }

void TextEdit::updateFont() {
  const bool usr = !fnt.isEmpty();
  if(usr==fntInUse && !usr)
    return;

  fntInUse = usr;
  if(fntInUse)
    textM.setFont(fnt); else
    textM.setFont(Application::font());
  }

void TextEdit::undo() {
  stk.undo(textM);
  update();
  }

void TextEdit::redo() {
  stk.redo(textM);
  update();
  }

void TextEdit::setText(const char *text) {
  textM.setText(text);
  invalidateSizeHint();
  update();
  }

void TextEdit::setFont(const Font &f) {
  fnt = f;
  invalidateSizeHint();
  update();
  }

const Font& TextEdit::font() const {
  return fnt;
  }

void TextEdit::setTextColor(const Color& cl) {
  fntCl = cl;
  update();
  }

const Color& TextEdit::textColor() const {
  return fntCl;
  }

TextModel::Cursor TextEdit::selectionStart() const {
  return selS;
  }

TextModel::Cursor TextEdit::selectionEnd() const {
  return selE;
  }

void TextEdit::setUndoRedoEnabled(bool enable) {
  undoEnable = enable;
  }

bool TextEdit::isUndoRedoEnabled() const {
  return undoEnable;
  }

void TextEdit::mouseDownEvent(MouseEvent &e) {
  updateFont();
  auto& m = margins();
  selS = textM.charAt(e.pos()-Point(m.left,m.top));
  selE = selS;
  update();
  }

void TextEdit::mouseDragEvent(MouseEvent &e) {
  updateFont();
  auto& m = margins();
  selE = textM.charAt(e.pos()-Point(m.left,m.top));
  update();
  }

void TextEdit::mouseUpEvent(MouseEvent& e) {
  e.accept();
  }

void TextEdit::keyDownEvent(KeyEvent& e) {
  keyEventImpl(e);
  }

void TextEdit::keyRepeatEvent(KeyEvent& e) {
  keyEventImpl(e);
  }

void TextEdit::keyUpEvent(KeyEvent&) {
  }

void TextEdit::keyEventImpl(KeyEvent& e) {
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
      }
    }
  selE = selS;
  update();
  }

void TextEdit::paintEvent(PaintEvent &e) {
  Painter p(e);
  style().draw(p, this,  Style::E_Background,       state(), Rect(0,0,w(),h()), Style::Extra(*this));
  style().draw(p, textM, Style::TE_TextEditContent, state(), Rect(0,0,w(),h()), Style::Extra(*this));
  }

void TextEdit::focusEvent(FocusEvent& e) {
  if(e.in)
    anim.start(Style::cursorFlashTime);
  }

void TextEdit::onRepaintCursor() {
  if(!hasFocus() && !cursorState) {
    anim.stop();
    return;
    }
  cursorState = !cursorState;
  update();
  }
