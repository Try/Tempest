#include "widget.h"

#include <Tempest/Layout>

using namespace Tempest;

Widget::Widget() {
  new(layBuf) Layout();

  }

Widget::~Widget() {
  std::vector<Widget*> rm=std::move(wx);
  for(auto& w:rm)
    w->ow=nullptr;
  for(auto& w:rm)
    delete w;
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
  for(size_t i=0;i<count;++i)
    widget(i).paintEvent(e);
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
  return lay->takeWidget(w);
  }

void Widget::setGeometry(const Rect &rect) {
  if(wrect==rect)
    return;
  wrect=rect;
  lay->applyLayout();
  }

void Widget::setGeometry(int x, int y, int w, int h) {
  setGeometry(Rect(x,y,w,h));
  }

void Widget::setSizeHint(const Size &s) {
  if(szHint==s)
    return;
  szHint=s;
  lay->applyLayout();
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

void Widget::paintEvent(PaintEvent&) {
  }
