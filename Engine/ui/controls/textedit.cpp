#include "textedit.h"

#include <Tempest/Painter>
#include <Tempest/Application>
#include <Tempest/TextCodec>

using namespace Tempest;

TextEdit::TextEdit() {
  textM.setFont(Application::font());
  setFocusPolicy(StrongFocus);

  anim.timeout.bind(this,&Widget::update);
  anim.start(500);
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
  selS = textM.charAt(e.pos());
  selE = selS;
  update();
  }

void TextEdit::mouseDragEvent(MouseEvent &e) {
  updateFont();
  selE = textM.charAt(e.pos());
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
  if(e.key==Event::K_Left) {
    selS = textM.advance(selS,-1);
    }
  else if(e.key==Event::K_Right) {
    selS = textM.advance(selS, 1);
    }
  else if(e.key==Event::K_Delete || e.key==Event::K_Back) {
    TextModel::Cursor end = selE;
    if(selS==selE) {
      end = textM.advance(selS, e.key==Event::K_Delete ? 1 : -1);
      }
    if(textM.isValid(end))
      selS = textM.erase(selS,end);
    }
  else {
    char t[3] = {};
    if(e.key==Event::K_Return) {
      t[0] = '\n';
      } else {
      TextCodec::toUtf8(e.code,t);
      }
    if(selS==selE)
      selS = textM.insert(t,selS); else
      selS = textM.replace(t,selS,selE);
    }
  selE = selS;
  update();
  }

void TextEdit::paintEvent(PaintEvent &e) {
  Painter p(e);
  style().draw(p, this,  Style::E_Background,       state(), Rect(0,0,w(),h()), Style::Extra(*this));
  style().draw(p, textM, Style::TE_TextEditContent, state(), Rect(0,0,w(),h()), Style::Extra(*this));
  }
