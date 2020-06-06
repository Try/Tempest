#include "textmodel.h"

#include <Tempest/Application>
#include <Tempest/Painter>
#include <Tempest/TextCodec>

#include <cstring>
#include "utility/utf8_helper.h"

using namespace Tempest;


TextModel::CommandInsert::CommandInsert(const char* txtIn, TextModel::Cursor where)
  : where(where) {
  size_t l = std::strlen(txtIn);
  if(l<=2) {
    txtShort[0] = txtIn[0];
    txtShort[1] = txtIn[1];
    } else {
    txt.assign(txtIn,txtIn+l);
    }
  }

void TextModel::CommandInsert::redo(TextModel& subj) {
  if(txtShort[0]=='\0')
    subj.insert(txt.data(),where); else
    subj.insert(txtShort,where);
  }

void TextModel::CommandInsert::undo(TextModel& subj) {
  if(txtShort[0]=='\0') {
    subj.erase(where,txt.size());
    } else {
    subj.erase(where,std::strlen(txtShort));
    }
  }


TextModel::CommandReplace::CommandReplace(const char* txtIn, TextModel::Cursor beg, TextModel::Cursor e)
  : begin(beg), end(e) {
  if(end.line<begin.line || (end.line==begin.line && end.offset<begin.offset))
    std::swap(begin,end);
  size_t l = std::strlen(txtIn);
  if(l<=2) {
    txtShort[0] = txtIn[0];
    txtShort[1] = txtIn[1];
    } else {
    txt.assign(txtIn,txtIn+l);
    }
  }

void TextModel::CommandReplace::redo(TextModel& subj) {
  subj.fetch(begin,end,prev);
  if(txtShort[0]=='\0')
    subj.replace(txt.data(),begin,end); else
    subj.replace(txtShort,  begin,end);
  }

void TextModel::CommandReplace::undo(TextModel& subj) {
  const char* t = txtShort;
  if(txtShort[0]=='\0')
    t = txt.data();

  size_t l = std::strlen(t);
  auto   e = subj.advance(begin,int(l));
  subj.replace(prev.data(), begin,e);
  }


TextModel::CommandErase::CommandErase(TextModel::Cursor beg, TextModel::Cursor e)
  : begin(beg), end(e) {
  if(end.line<begin.line || (end.line==begin.line && end.offset<begin.offset))
    std::swap(begin,end);
  }

void TextModel::CommandErase::redo(TextModel& subj) {
  auto s = subj.cursorCast(begin);
  auto e = subj.cursorCast(end);
  if(e-s<3) {
    subj.fetch(begin,end,prevShort);
    } else {
    subj.fetch(begin,end,prev);
    }
  subj.erase(begin,end);
  }

void TextModel::CommandErase::undo(TextModel& subj) {
  if(prevShort[0]=='\0')
    subj.insert(prev.data(),begin); else
    subj.insert(prevShort,begin);
  }


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

void TextModel::insert(const char* t, Cursor where) {
  if(txt.size()==0) {
    setText(t);
    }
  size_t at  = cursorCast(where);
  size_t len = std::strlen(t);

  txt.insert(txt.begin()+at,t,t+len);
  buildIndex();
  sz.actual=false;

  cursorCast(at+len);
  }

void TextModel::erase(Cursor cs, Cursor ce) {
  size_t s = cursorCast(cs);
  size_t e = cursorCast(ce);
  if(e<s)
    std::swap(s,e);

  txt.erase(txt.begin()+s,txt.begin()+e);
  buildIndex();
  sz.actual=false;
  }

void TextModel::erase(TextModel::Cursor s, size_t count) {
  auto e = advance(s,int(count));
  erase(s,e);
  }

void TextModel::replace(const char* t, TextModel::Cursor cs, TextModel::Cursor ce) {
  if(txt.size()==0) {
    setText(t);
    return;
    }
  size_t s   = cursorCast(cs);
  size_t e   = cursorCast(ce);
  size_t len = std::strlen(t);
  if(e<s)
    std::swap(s,e);

  txt.erase (txt.begin()+s,txt.begin()+e);
  txt.insert(txt.begin()+s,t,t+len);

  buildIndex();
  sz.actual=false;
  }

void TextModel::fetch(TextModel::Cursor cs, TextModel::Cursor ce, std::string& buf) {
  size_t s = cursorCast(cs);
  size_t e = cursorCast(ce);
  if(s==e)
    return;
  if(e<s)
    std::swap(s,e);
  buf.resize(e-s);
  std::memcpy(&buf[0],&txt[s],buf.size());
  }

void TextModel::fetch(TextModel::Cursor cs, TextModel::Cursor ce, char* buf) {
  size_t s = cursorCast(cs);
  size_t e = cursorCast(ce);
  if(s==e)
    return;
  if(e<s)
    std::swap(s,e);
  std::memcpy(&buf[0],&txt[s],e-s);
  }

TextModel::Cursor TextModel::advance(TextModel::Cursor src, int32_t offset) const {
  size_t c = cursorCast(src);
  if(offset>0) {
    const char* str = txt.data()+c, *end = txt.data()+txt.size();
    for(int32_t i=0;i<offset && str<end;++i) {
      const auto l = Detail::utf8LetterLength(str);
      str+=l;
      }
    return cursorCast(std::distance(txt.data(),str));
    } else {
    offset = -offset;
    const char* str = txt.data()+c, *begin = txt.data();
    for(int32_t i=0;i<offset;++i) {
      while(str!=begin) {
        str--;
        if((uint8_t(*str) >> 6) != 0x2)
          break;
        }
      }
    return cursorCast(std::distance(txt.data(),str));
    }
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

void TextModel::paint(Painter& p, int x, int y) const {
  paint(p,fnt,x,y);
  }

void TextModel::paint(Painter &p,const Font& fnt,int fx,int fy) const {
  float x = float(fx);
  float y = float(fy);

  auto pb=p.brush();
  Utf8Iterator i(txt.data());
  while(i.hasData()) {
    auto ch=i.next();
    if(ch=='\0'){
      p.setBrush(pb);
      return;
      }
    if(ch=='\n'){
      x =  0;
      y += fnt.pixelSize();
      continue;
      }

    auto l=fnt.letter(ch,p);
    if(!l.view.isEmpty()) {
      p.setBrush(Brush(l.view,Color(1.0),PaintDevice::Alpha));
      p.drawRect(int(x+l.dpos.x),int(y+l.dpos.y),l.view.w(),l.view.h());
      }

    x += l.advance.x;
    }
  p.setBrush(pb);
  }

void TextModel::calcSize() const {
  sz = calcSize(fnt);
  }

TextModel::Sz TextModel::calcSize(const Font& fnt) const {
  float x=0, w=0;
  int   y=0, top=0;

  Utf8Iterator i(txt.data());
  while(i.hasData()){
    char32_t ch = i.next();
    if(ch=='\n'){
      w = std::max(w,x);
      x = 0;
      y = 0;
      top+=int(fnt.pixelSize());
      } else {
      auto l=fnt.letterGeometry(ch);
      x += l.advance.x;
      y =  std::max(-l.dpos.y,y);
      }
    }
  w = std::max(w,x);

  Sz sz;
  sz.wrapHeight=y+top;
  sz.sizeHint  =Size(int(std::ceil(w)),top+int(fnt.pixelSize()));
  return sz;
  }

void TextModel::buildIndex() {
  if(txt.size()==0) {
    line.clear();
    return;
    }

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

  line[ln].txt  = txt.data()+beg;
  line[ln].size = txt.size()-beg-1;
  }

TextModel::Cursor TextModel::charAt(int x, int y) const {
  if(line.size()==0) {
    Cursor c;
    c.line   = 0;
    c.offset = 0;
    return c;
    }

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
  int    px      = 0;
  size_t prevPos = 0;
  while(i.hasData()){
    prevPos = i.pos();
    char32_t ch = i.next();
    auto l=fnt.letterGeometry(ch);
    if(px<=x && x<=px+l.advance.x) {
      if(x<=px+l.advance.x/2)
        c.offset = prevPos; else
        c.offset = i.pos();
      return c;
      }
    px += l.advance.x;
    }
  c.offset = ln.size;
  return c;
  }

Point TextModel::mapToCoords(Cursor c) const {
  if(!isValid(c))
    return Point();
  Point p;
  p.y = int(c.line*fnt.pixelSize());
  auto& ln = line[c.line];

  Utf8Iterator str(ln.txt,ln.size);
  while(str.hasData() && str.pos()<c.offset){
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

size_t TextModel::cursorCast(Cursor c) const {
  if(line.size()==0)
    return 0;
  size_t r=std::distance(txt.data(),line[c.line].txt);
  return r+c.offset;
  }

TextModel::Cursor TextModel::cursorCast(size_t c) const {
  Cursor cx;
  for(size_t i=0;i<line.size();++i) {
    size_t b = std::distance(txt.data(),line[i].txt);

    if(b<=c && c<=b+line[i].size) {
      cx.line   = i;
      cx.offset = c-b;
      return cx;
      }
    }
  return cx;
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

void TextModel::drawCursor(Painter &p, int x, int y, Cursor s, Cursor e) const {
  if(!isValid(s) || !isValid(e))
    return;

  auto b = p.brush();
  p.setBrush(Color(0,0,1,1));
  if(s.line>e.line)
    std::swap(s,e);
  Cursor s1 = s;
  s1.offset = line[s.line].size;

  int lnH = int(fnt.pixelSize());
  if(s.line!=e.line) {
    auto posS0 = mapToCoords(s);
    auto posS1 = mapToCoords(s1);
    auto posE  = mapToCoords(e);

    p.drawRect(x+posS0.x,y+posS0.y,posS1.x-posS0.x,lnH);
    p.drawRect(x,        y+posE.y,posE.x,          lnH);
    for(size_t ln=s.line+1;ln<e.line;++ln) {
      Cursor cx;
      cx.line   = ln;
      cx.offset = line[ln].size;
      auto posLn = mapToCoords(cx);
      p.drawRect(x,y+posLn.y,posLn.x,lnH);
      }
    } else {
    auto posS = mapToCoords(s);
    auto posE = mapToCoords(e);
    int is = std::min(posS.x,posE.x);
    int ie = std::max(posS.x,posE.x);
    p.drawRect(x+is,y+posS.y,x+ie-is,lnH);
    }

  p.setBrush(b);
  }
