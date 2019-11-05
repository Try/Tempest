#include "textmodel.h"

#include <cstring>

#include <Tempest/Painter>
#include "utility/utf8_helper.h"

using namespace Tempest;

TextModel::TextModel(const char *str)
  :txt(std::strlen(str)+1){
  std::memcpy(txt.data(),str,txt.size());
  buildIndex();
  }

void TextModel::setText(const char *str) {
  txt.resize(std::strlen(str)+1);
  std::memcpy(txt.data(),str,txt.size());
  buildIndex();
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
  return txt.size()<=1;
  }

void TextModel::paint(Painter &p,int fx,int fy) const {
  auto  b = p.brush();
  float x = fx;
  int   y = fy;

  Utf8Iterator i(txt.data());
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

  Utf8Iterator i(txt.data());
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

void TextModel::buildIndex() {
  size_t count=0;
  for(auto c:txt)
    if(c=='\n')
      count++;
  line.resize(count+1);

  size_t beg=0;
  size_t ln =0;
  Utf8Iterator i(txt.data());
  while(i.hasData()){
    char32_t ch = i.next();
    if(ch=='\n') {
      size_t ipos=i.pos();
      line[ln].txt  = &txt[beg];
      line[ln].size = ipos-1-beg;

      beg=ipos;
      ln++;
      }
    }

  line[ln].txt  = &txt[beg];
  line[ln].size = txt.size()-beg-1;
  }

TextModel::Cursor TextModel::charAt(int x, int y) const {
  if(line.size()==0)
    return Cursor();

  if(y<0)
    y=0;
  if(x<0)
    x=0;
  Cursor c;
  c.line   = size_t(y/fnt.pixelSize());
  if(c.line>=line.size())
    c.line=line.size()-1;

  auto& ln = line[c.line];
  Utf8Iterator i(ln.txt,ln.size);
  int px=0;
  while(i.hasData()){
    char32_t ch = i.next();
    auto l=fnt.letterGeometry(ch);
    if(px<x && x<=px+l.advance.x) {
      c.offset = i.pos();
      if(x<=px+l.advance.x/2)
        c.offset--;
      return c;
      }
    px += l.advance.x;
    }
  c.offset = ln.size;
  return c;
  }

Point TextModel::mapToCoords(TextModel::Cursor c) const {
  if(!isValid(c))
    return Point();
  Point p;
  p.y = int(c.line*fnt.pixelSize());
  auto& ln = line[c.line];
  Utf8Iterator str(ln.txt,ln.size);

  for(size_t i=0;str.hasData() && i<c.offset;++i){
    char32_t ch = str.next();
    auto l=fnt.letterGeometry(ch);
    p.x += l.advance.x;
    }
  return p;
  }

bool TextModel::isValid(TextModel::Cursor c) const {
  if(c.line>=line.size())
    return false;
  return c.offset<=line[c.line].size;
  }

void TextModel::drawCursor(Painter& p, int x, int y,TextModel::Cursor c) const {
  if(!isValid(c))
    return;

  auto pos = mapToCoords(c)+Point(x,y);

  auto b = p.brush();
  p.setBrush(Color(0,0,1,1));
  p.drawRect(pos.x,pos.y,3,int(fnt.pixelSize()));
  p.setBrush(b);
  }

void TextModel::drawCursor(Painter &p, int x, int y, TextModel::Cursor s, TextModel::Cursor e) const {
  if(!isValid(s) || !isValid(e))
    return;

  auto b = p.brush();
  p.setBrush(Color(0,0,1,1));
  if(s.line>e.line)
    std::swap(s,e);

  if(s.line!=e.line) {
    auto t = std::min(int(s.line),int(e.line))+1;
    auto b = std::max(int(s.line),int(e.line))-1;

    if(t<=b)
      p.drawRect(x,y+int(t*fnt.pixelSize()),w(),int(b*fnt.pixelSize()));
    auto posS = mapToCoords(s);
    p.drawRect(x+posS.x,y+posS.y,w()-posS.x,int(fnt.pixelSize()));
    auto posE = mapToCoords(e);
    p.drawRect(x,y+posE.y,posE.x,int(fnt.pixelSize()));
    } else {
    auto posS = mapToCoords(s);
    auto posE = mapToCoords(e);
    int is = std::min(posS.x,posE.x);
    int ie = std::max(posS.x,posE.x);
    p.drawRect(x+is,y+posS.y,x+ie-is,int(fnt.pixelSize()));
    }

  p.setBrush(b);
  }
