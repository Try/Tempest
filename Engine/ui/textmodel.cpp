#include "textmodel.h"

#include <cstring>

#include <Tempest/Painter>
#include "utility/utf8_helper.h"

using namespace Tempest;

TextModel::TextModel(const char *txt)
  :text(std::strlen(txt)+1){
  std::memcpy(text.data(),txt,text.size());
  }

void TextModel::setText(const char *txt) {
  text.resize(std::strlen(txt)+1);
  std::memcpy(text.data(),txt,text.size());
  sz.actual=false;
  }

void TextModel::setFont(const Font &f) {
  fnt      =f;
  sz.actual=false;
  }

const Font& TextModel::font() const {
  return fnt;
  }

const Size &TextModel::sizeHint() const {
  if(!sz.actual)
    calcSize();
  return sz.sizeHint;
  }

Size TextModel::wrapSize() const {
  if(!sz.actual)
    calcSize();
  return Size(sz.sizeHint.w,sz.wrapHeight);
  }

bool TextModel::isEmpty() const {
  return text.size()<=1;
  }

void TextModel::paint(Painter &p,int fx,int fy) const {
  auto  b = p.brush();
  float x = fx;
  int   y = fy;

  Utf8Iterator i(text.data());
  while(i.hasData()){
    char32_t ch = i.next();
    auto l=fnt.letter(ch,p);
    if(ch=='\n'){
      x =  0;
      y += fnt.pixelSize();
      } else {
      if(!l.view.isEmpty()) {
        p.setBrush(l.view);
        p.drawRect(int(x+l.dpos.x),y+l.dpos.y,l.view.w(),l.view.h());
        }
      x += l.advance.x;
      }
    }

  p.setBrush(b);
  }

void TextModel::calcSize() const {
  float x=0, w=0;
  int   y=0, top=0;

  Utf8Iterator i(text.data());
  while(i.hasData()){
    char32_t ch = i.next();
    if(ch=='\n'){
      w = std::max(w,x);
      x = 0;
      y = 0;
      top+=fnt.pixelSize();
      } else {
      auto l=fnt.letterGeometry(ch);
      x += l.advance.x;
      y =  std::max(-l.dpos.y,y);
      }
    }
  w = std::max(w,x);

  sz.wrapHeight=y+top;
  sz.sizeHint  =Size(int(std::ceil(w)),top+int(fnt.pixelSize()));
  sz.actual    =true;
  }
