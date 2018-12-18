#include "textmodel.h"

#include <cstring>

#include <Tempest/Painter>

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

void TextModel::paint(Painter &p,int x,int y) const {
  p.setFont(fnt);

  const char* txt=text.data();
  p.drawText(x,y,txt);
  }

void TextModel::calcSize() const {
  float x=0;
  int   y=0;

  const char* txt=text.data();
  if(txt!=nullptr) {
    for(;*txt;++txt) {
      auto& l=fnt.letterGeometry(*txt); //TODO: utf8
      x += l.advance.x;
      y =  std::max(-l.dpos.y,y);
      }
    }

  sz.wrapHeight=y;
  sz.sizeHint  =Size(int(std::ceil(x)),int(std::ceil(fnt.pixelSize())));
  sz.actual    =true;
  }
