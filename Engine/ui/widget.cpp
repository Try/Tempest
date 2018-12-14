#include "widget.h"

#include <Tempest/Layout>

using namespace Tempest;

Widget::Widget() {
  new(layBuf) Layout();
  }

Widget::~Widget() {
  removeAllWidgets();
  freeLayout();
  }

void Widget::setLayout(Orienration ori) noexcept {
  static_assert(sizeof(LinearLayout)<=sizeof(layBuf),"layBuf is too small");
  freeLayout();
  new(layBuf) LinearLayout(ori);
  lay=reinterpret_cast<Layout*>(layBuf);
  lay->bind(this);
  }

void Widget::setLayout(Layout *l) {
  if(l==nullptr)
    throw std::invalid_argument("null layout");
  freeLayout();
  lay = l;
  lay->bind(this);
  }

void Widget::applyLayout() {
  lay->applyLayout();
  }

void Widget::removeAllWidgets() {
  std::vector<Widget*> rm=std::move(wx);
  for(auto& w:rm)
    w->ow=nullptr;
  for(auto& w:rm)
    delete w;
  }

void Widget::freeLayout() noexcept {
  if(reinterpret_cast<char*>(lay)!=layBuf){
    delete lay;
    } else {
    layout().~Layout();
    }
  }

void Widget::dispatchPaintEvent(PaintEvent& e) {
  paintEvent(e);
  const size_t count=widgetsCount();
  for(size_t i=0;i<count;++i) {
    Widget& wx=widget(i);

    PaintEvent ex(e,wx.x(),wx.y(),wx.w(),wx.h());
    wx.dispatchPaintEvent(ex);
    }
  }

void Widget::setOwner(Widget *w) {
  if(ow==w)
    return;
  if(ow)
    ow->takeWidget(this);
  ow=w;
  }

Widget& Widget::addWidget(Widget *w) {
  if(w==nullptr)
    throw std::invalid_argument("null widget");
  wx.emplace_back(w);
  w->setOwner(this);
  lay->applyLayout();
  return *w;
  }

Widget *Widget::takeWidget(Widget *w) {
  if(state.mouseFocus==w)
    state.mouseFocus=nullptr;
  return lay->takeWidget(w);
  }

void Widget::setGeometry(const Rect &rect) {
  if(wrect==rect)
    return;
  bool resize=(wrect.w!=rect.w || wrect.h!=rect.h);
  wrect=rect;

  if(resize) {
    lay->applyLayout();
    SizeEvent e(uint32_t(rect.w),uint32_t(rect.h));
    resizeEvent( e );
    }
  }

void Widget::setGeometry(int x, int y, int w, int h) {
  setGeometry(Rect(x,y,w,h));
  }

void Widget::resize(const Size &size) {
  resize(size.w,size.h);
  }

void Widget::resize(int w, int h) {
  if(wrect.w==w && wrect.h==h)
    return;
  wrect.w=w;
  wrect.h=h;

  lay->applyLayout();
  SizeEvent e(w,h);
  resizeEvent( e );
  }

void Widget::setSizeHint(const Size &s) {
  if(szHint==s)
    return;
  szHint=s;
  lay->applyLayout();
  }

void Widget::setSizeHint(const Size &s, const Margin &add) {
  setSizeHint(Size(s.w+add.xMargin(),s.h+add.yMargin()));
  }

void Widget::setSizePolicy(SizePolicyType h, SizePolicyType v) {
  if(szPolicy.typeH==h && szPolicy.typeV==v)
    return;
  szPolicy.typeH=h;
  szPolicy.typeV=v;

  lay->applyLayout();
  }

void Widget::setSizePolicy(const SizePolicy &sp) {
  if(szPolicy==sp)
    return;
  szPolicy=sp;
  lay->applyLayout();
  }

void Widget::setMargins(const Margin &m) {
  if(marg!=m){
    marg=m;
    lay->applyLayout();
    }
  }

void Widget::setSpacing(int s) {
  if(s!=spa){
    spa=s;
    lay->applyLayout();
    }
  }

void Widget::setMaximumSize(const Size &s) {
  if(szPolicy.maxSize==s)
    return;

  szPolicy.maxSize = s;

  if( owner() )
    owner()->applyLayout();

  if(wrect.w>s.w || wrect.h>s.h)
    setGeometry( wrect.x, wrect.y, std::min(s.w, wrect.w), std::min(s.h, wrect.h) );
  }

void Widget::setMinimumSize(const Size &s) {
  if(szPolicy.minSize==s)
    return;

  szPolicy.minSize = s;
  if( owner() )
    owner()->applyLayout();

  if(wrect.w<s.w || wrect.h<s.h )
    setGeometry( wrect.x, wrect.y, std::max(s.w, wrect.w), std::max(s.h, wrect.h) );
  }

void Widget::setMaximumSize(int w, int h) {
  setMaximumSize( Size(w,h) );
  }

void Widget::setMinimumSize(int w, int h) {
  setMinimumSize( Size(w,h) );
  }

Rect Widget::clentRet() const {
  return Rect(marg.left,marg.right,wrect.w-marg.xMargin(),wrect.h-marg.yMargin());
  }

void Widget::paintEvent(PaintEvent&) {
  }

void Widget::resizeEvent(SizeEvent &e) {
  e.accept();
  }

void Widget::mouseDownEvent(MouseEvent &e) {
  e.ignore();
  }

void Widget::mouseUpEvent(MouseEvent &e) {
  e.ignore();
  }

void Widget::mouseMoveEvent(MouseEvent &e) {
  e.ignore();
  }

void Widget::dispatchMoveEvent(MouseEvent &event) {
  Point pos=event.pos();
  for(auto i:wx){
    if(i->rect().contains(pos)){
      MouseEvent ex(event.x - i->x(),
                    event.y - i->y(),
                    event.button,
                    event.delta,
                    event.mouseID,
                    event.type());
      i->dispatchMoveEvent(ex);
      if(ex.isAccepted()) {
        if(ex.type()==Event::MouseDown)
          state.mouseFocus=i;
        event.accept();
        return;
        }
      }
    }

  switch(event.type()){
    case Event::MouseDown: mouseDownEvent(event); break;
    case Event::MouseUp:   mouseUpEvent  (event); break;
    case Event::MouseMove: mouseMoveEvent(event); break;
    default:break;
    }
  }
