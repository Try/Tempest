#include "style.h"

#include <Tempest/Platform>

#include <Tempest/Application>
#include <Tempest/Painter>
#include <Tempest/Widget>
#include <Tempest/Button>
#include <Tempest/Icon>
#include <Tempest/TextEdit>
#include <Tempest/Label>
// #include <Tempest/CheckBox>

#include <cassert>

using namespace Tempest;

Style::UiMetrics::UiMetrics() {
#ifdef __MOBILE_PLATFORM__
  buttonSize = 48;
#else
  buttonSize = 27;
#endif
  }

const Margin Style::Extra::emptyMargin;
const Icon   Style::Extra::emptyIcon;
const Font   Style::Extra::emptyFont;
const Color  Style::Extra::emptyColor;

Style::Extra::Extra(const Widget &owner)
  : margin(owner.margins()), icon(emptyIcon), fontColor(emptyColor), spacing(owner.spacing()) {
  }

Style::Extra::Extra(const Button &owner)
  : margin(owner.margins()), icon(owner.icon()), fontColor(emptyColor), spacing(owner.spacing()) {
  }

Style::Extra::Extra(const Label &owner)
  : margin(owner.margins()), icon(emptyIcon), fontColor(owner.textColor()), spacing(owner.spacing()) {
  }

Style::Extra::Extra(const AbstractTextInput& owner)
  : margin(owner.margins()), icon(emptyIcon), fontColor(owner.textColor()), spacing(owner.spacing()),
    selectionStart(owner.selectionStart()), selectionEnd(owner.selectionEnd()){
  }

Style::Style() {
  }

Style::~Style() {
  assert(polished==0);
  }

Style::UIIntefaceIdiom Style::idiom() const {
#ifdef __MOBILE_PLATFORM__
  return UIIntefaceIdiom(UIIntefacePhone);
#else
  return UIIntefaceIdiom(UIIntefaceUnspecified);
#endif
  }

void Style::polish  (Widget&) const {
  polished++;
  }

void Style::unpolish(Widget&) const {
  polished--;
  }

const Tempest::Sprite &Style::iconSprite(const Icon& icon,const WidgetState &st, const Rect &r) {
  const int sz = std::min(r.w,r.h);
  return icon.sprite(sz,sz,st.disabled ? Icon::ST_Disabled : Icon::ST_Normal);
  }

void Style::draw(Painter& ,Widget*, Element, const WidgetState&, const Rect &, const Extra&) const {
  }

void Style::draw(Painter &p, Panel* w, Element e, const WidgetState &st, const Rect &r, const Extra& extra) const {
  (void)w;
  (void)st;
  (void)e;
  (void)extra;

  p.translate(r.x,r.y);

  p.setBrush(Color(0.8f,0.8f,0.85f,0.75f));
  p.drawRect(0,0,r.w,r.h);

  p.setPen(Color(0.25,0.25,0.25,1));

  p.drawLine(0,0,    r.w-1,0);
  p.drawLine(0,r.h-1,r.w-1,r.h-1);

  p.drawLine(0,    0,0    ,r.h-1);
  p.drawLine(r.w-1,0,r.w-1,r.h  );

  p.translate(-r.x,-r.y);
  }

void Style::draw(Painter& p, Dialog* w, Style::Element e,
                 const WidgetState& st, const Rect& r, const Style::Extra& extra) const {
  draw(p,reinterpret_cast<Panel*>(w),e,st,r,extra);
  }

void Style::draw(Painter& p, Button *w, Element e, const WidgetState &st, const Rect &r, const Extra &extra) const {
  (void)w;
  (void)e;
  (void)extra;

  auto pen   = p.pen();
  auto brush = p.brush();
  p.translate(r.x,r.y);

  const Button::Type buttonType=st.button;

  const bool drawBackFrame = (buttonType!=Button::T_ToolButton || st.moveOver) &&
                             (buttonType!=Button::T_FlatButton || st.pressed) &&
                             (e!=E_MenuItemBackground);
  if( drawBackFrame ) {
    if(st.pressed || st.checked!=WidgetState::Unchecked)
      p.setBrush(Color(0.4f,0.4f,0.45f,0.75f)); else
    if(st.moveOver)
      p.setBrush(Color(0.5f,0.5f,0.55f,0.75f)); else
      p.setBrush(Color(0.8f,0.8f,0.85f,0.75f));
    p.drawRect(0,0,r.w,r.h);

    p.setPen(Color(0.25,0.25,0.25,1));
    p.drawLine(0,0,    r.w-1,0    );
    p.drawLine(0,r.h-1,r.w-1,r.h-1);
    p.drawLine(0,      0,    0,r.h-1);
    p.drawLine(r.w-1,  0,r.w-1,r.h  );
    }

  if( e==E_ArrowUp || e==E_ArrowDown || e==E_ArrowLeft || e==E_ArrowRight ){
    p.setPen(Color(0.1f,0.1f,0.15f,1));

    int cx=r.w/2;
    int cy=r.h/2;
    const int dx=(r.w-4)/2;
    const int dy=(r.h-4)/2;

    if(e==E_ArrowUp) {
      cy-=dy/2;
      p.drawLine(cx-dx,cy+dy, cx,cy);
      p.drawLine(cx,cy, cx+dx,cy+dy);
      } else
    if(e==E_ArrowDown) {
      cy+=dy/2;
      p.drawLine(cx-dx,cy-dy, cx,cy);
      p.drawLine(cx,cy, cx+dx,cy-dy);
      } else
    if(e==E_ArrowLeft) {
      cx-=dx/2;
      p.drawLine(cx+dx, cy-dy, cx,    cy);
      p.drawLine(cx,    cy,    cx+dx, cy+dy);
      } else
    if(e==E_ArrowRight) {
      cx+=dx/2;
      p.drawLine(cx-dx,cy-dy, cx,cy);
      p.drawLine(cx,cy, cx-dx,cy+dy);
      }
    }

  p.translate(-r.x,-r.y);
  p.setBrush(brush);
  p.setPen(pen);
  }

void Style::draw(Painter &p, CheckBox *w, Element e, const WidgetState &st, const Rect &r, const Style::Extra &extra) const {
  (void)w;
  (void)e;
  (void)extra;

  const int  s  = std::min(r.w,r.h);
  const Size sz = Size(s,s);

  p.translate(r.x,r.y);
  p.setBrush(Color(0.25,0.25,0.25,1));

  p.drawLine(0,     0,sz.w-1,     0);
  p.drawLine(0,sz.h-1,sz.w-1,sz.h-1);

  p.drawLine(     0,0,     0,sz.h-1);
  p.drawLine(sz.w-1,0,sz.w-1,sz.h  );

  p.setBrush(Color(1));

  if( st.checked==WidgetState::Checked ) {
    int x = 0,
        y = (r.h-sz.h)/2;
    int d = st.pressed ? 2 : 4;
    if( st.pressed ) {
      p.drawLine(x+d, y+d, x+sz.w-d, y+sz.h-d);
      p.drawLine(x+sz.w-d, y+d, x+d, y+sz.h-d);
      } else {
      p.drawLine(x, y, x+sz.w, y+sz.h);
      p.drawLine(x+sz.w, y, x, y+sz.h);
      }
    } else
  if( st.checked==WidgetState::PartiallyChecked ) {
    int x = 0,
        y = (r.h-sz.h)/2;
    int d = st.pressed ? 2 : 4;
    p.drawLine(x+d, y+d,      x+sz.w-d, y+d);
    p.drawLine(x+d, y+sz.h+d, x+sz.w-d, y+sz.h+d);

    p.drawLine(x+d, y+d, x+d, y+sz.h-d);
    p.drawLine(x+sz.w+d, y+d, x+sz.w+d, y+sz.h-d);
    }
  p.translate(-r.x,-r.y);
  }

void Style::draw(Painter &p, Label *w, Element e, const WidgetState &st, const Rect &r, const Style::Extra &extra) const {
  (void)p;
  (void)w;
  (void)e;
  (void)st;
  (void)r;
  (void)extra;
  // nop
  }

void Style::draw(Painter &p, AbstractTextInput* w, Element e, const WidgetState &st, const Rect &r, const Style::Extra &extra) const {
  (void)p;
  (void)w;
  (void)e;
  (void)st;
  (void)r;
  (void)extra;
  }

void Style::draw(Painter &p, ScrollBar*, Element e, const WidgetState &st, const Rect &r, const Style::Extra &extra) const {
  if(e==E_Background)
    return;
  draw(p,static_cast<Button*>(nullptr),e,st,r,extra);
  }

void Style::draw(Painter &p, const std::u16string &text, Style::TextElement e,
                 const WidgetState &st, const Rect &r, const Style::Extra &extra) const {
  /*
  const Margin& m = extra.margin;

  p.translate(r.x,r.y);

  const Sprite& icon = iconSprite(extra.icon,st,r);
  int dX=0;

  if( !icon.size().isEmpty() ){
    p.setBrush(icon);
    if( text.empty() ) {
      p.drawRect( (r.w-icon.w())/2, (r.h-icon.h())/2, icon.w(), icon.h() );
      } else {
      p.drawRect( m.left, (r.h-icon.h())/2, icon.w(), icon.h() );
      dX=icon.w();
      }
    }

  if(e==TE_CheckboxTitle){
    const int s = std::min(r.w,r.h);
    if( dX<s )
      dX=s;
    }

  if(dX!=0)
    dX+=8; // padding

  auto& fnt = textFont(text,st);
  p.setFont (extra.font);
  p.setBrush(extra.fontColor);
  const int h=extra.font.textSize(text).h;
  int flag=AlignBottom;
  if(e==TE_ButtonTitle)
    flag |= AlignHCenter;

  p.drawText( m.left+dX, (r.h-h)/2, r.w-m.xMargin()-dX, h, text, flag );

  p.translate(-r.x,-r.y);*/
  }

void Style::draw(Painter &p, const TextModel &text, Style::TextElement e,
                 const WidgetState &st, const Rect &r, const Style::Extra &extra) const {
  (void)e;

  const Margin& m  = extra.margin;
  const Rect    sc = p.scissor();

  p.setScissor(sc.intersected(Rect(m.left, 0, r.w-m.xMargin(), r.h)));
  p.translate(m.left,m.top);

  const Sprite& icon = iconSprite(extra.icon,st,r);
  int dX = 0;

  if(!icon.size().isEmpty()){
    p.setBrush(icon);
    if( text.isEmpty() ) {
      p.drawRect( (r.w-icon.w())/2, (r.h-icon.h())/2, icon.w(), icon.h() );
      } else {
      p.drawRect( 0, (r.h-icon.h())/2, icon.w(), icon.h() );
      dX=icon.w();
      }
    }

  if(e==TE_CheckboxTitle) {
    const int s = std::min(r.w,r.h);
    if( dX<s )
      dX=s;
    }

  if(dX!=0)
    dX+=extra.spacing;

  auto&     fnt   = text.font();
  const int fntSz = int(fnt.metrics().ascent);

  Point at;
  if(e!=TE_TextEditContent && e!=TE_LineEditContent && false) {
    const int h = text.wrapSize().h;
    at = {dX, r.h-(r.h-h)/2};
    } else {
    at = { 0, fntSz};
    }

  if(st.focus && extra.selectionStart==extra.selectionEnd) {
    if((Application::tickCount()/cursorFlashTime)%2)
      text.drawCursor(p,at.x,at.y-fntSz,extra.selectionStart);
    }

  if(extra.selectionStart!=extra.selectionEnd) {
    text.drawCursor(p,at.x,at.y-fntSz,extra.selectionStart,extra.selectionEnd);
    }
  text.paint(p, fnt, at.x, at.y);
  p.translate(-m.left,-m.top);
  p.setScissor(sc);
  }

Size Style::sizeHint(Widget*, Style::Element e, const TextModel* text, const Style::Extra& extra) const {
  (void)e;
  (void)text;
  (void)extra;
  return Size();
  }

Size Style::sizeHint(Panel*, Style::Element e, const TextModel* text, const Style::Extra& extra) const {
  (void)e;
  (void)text;
  (void)extra;
  return Size();
  }

Size Style::sizeHint(Button*, Style::Element e, const TextModel* text, const Style::Extra& extra) const {
  (void)e;

  Size  sz   = text!=nullptr ? text->sizeHint() : Size();
  auto& icon = extra.icon.sprite(SizePolicy::maxWidgetSize().w,
                                 sz.h+extra.margin.yMargin(),
                                 Icon::ST_Normal);
  Size  is   = icon.size();

  if(is.w>0)
    sz.w = sz.w+is.w+extra.spacing;
  sz.h = std::max(sz.h,is.h);
  return sz;
  }

Size Style::sizeHint(Label*, Style::Element e, const TextModel* text, const Style::Extra& extra) const {
  (void)e;
  (void)extra;
  Size sz = text!=nullptr ? text->sizeHint() : Size();
  return sz;
  }

const Style::UiMetrics& Style::metrics() const {
  static UiMetrics m;
  return m;
  }

Style::Element Style::visibleElements() const {
  return E_All;
  }

void Style::drawCursor(Painter &p,const WidgetState &st,int x,int h,bool animState) const {
  if( st.editable && animState && st.focus ){
    p.setBrush( Brush(Color(0,0,1,1),Painter::NoBlend) );
    p.drawRect( x-1, 0, 2, h );
    return;
    }
  }

void Style::implDecRef() const {
  refCnt--;
  if(refCnt==0)
    delete this;
  }

Style::UIIntefaceIdiom::UIIntefaceIdiom(Style::UIIntefaceCategory category):category(category){
  touch=(category==UIIntefacePad || category==UIIntefacePhone);
  }

