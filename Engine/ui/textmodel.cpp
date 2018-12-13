#include "textmodel.h"

#include <cstring>

using namespace Tempest;

TextModel::TextModel(const char *txt)
  :text(std::strlen(txt)+1){
  std::memcpy(text.data(),txt,text.size());
  }

void TextModel::setFont(const Font &f) {
  fnt      =f;
  sz.actual=false;
  }

const Size &TextModel::sizeHint() const {
  if(!sz.actual)
    calcSize();
  return sz.sizeHint;
  }

void TextModel::calcSize() const {
  float x=0;

  const char* txt=text.data();
  for(;*txt;++txt) {
    auto& l=fnt.letterGeometry(*txt); //TODO: utf8
    x += l.advance.x;
    }

  sz.sizeHint=Size(int(std::ceil(x)),int(std::ceil(fnt.pixelSize())));
  sz.actual  =true;
  }
