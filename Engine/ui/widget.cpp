#include "widget.h"

#include <Tempest/Layout>

using namespace Tempest;

Widget::Iterator::Iterator(Widget* owner)
  :owner(owner),nodes(&owner->wx){
  owner->iterator=this;
  }

Widget::Iterator::~Iterator(){
  if(owner!=nullptr)
    owner->iterator=nullptr;
  }

void Widget::Iterator::onDelete() {
  deleteLater=std::move(*nodes);
  nodes=&deleteLater;
  owner=nullptr;
  }

void Widget::Iterator::onDelete(size_t i,Widget* wx){
  if(i<=id)
    id--;
  if(getPtr==wx)
    getPtr=nullptr;
  }

bool Widget::Iterator::hasNext() const {
  return id<nodes->size();
  }

void Widget::Iterator::next() {
  ++id;
  }

Widget* Widget::Iterator::getLast() {
  return getPtr;
  }

Widget* Widget::Iterator::get() {
  getPtr=(*nodes)[id];
  return getPtr;
  }


Widget::Widget() {
  lay = new(layBuf) Layout();
  lay->bind(this);
  }

Widget::~Widget() {
  if(ow)
    ow->takeWidget(this);
  if(iterator!=nullptr)
    iterator->onDelete();
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

  state.focus     =nullptr;

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

void Widget::implDisableSum(Widget *root,int diff) noexcept {
  root->state.disable += diff;

  const std::vector<Widget*> & w = root->wx;

  for( Widget* wx:w )
    implDisableSum(wx,diff);
  }

void Widget::dispatchPaintEvent(PaintEvent& e) {
  paintEvent(e);
  const size_t count=widgetsCount();
  for(size_t i=0;i<count;++i) {
    Widget& wx=widget(i);

    Rect r = e.viewPort();
    r.x -= wx.x();
    r.y -= wx.y();
    Rect sc = r.intersected(Rect(0,0,wx.w(),wx.h()));

    PaintEvent ex(e,wx.x(),wx.y(),sc.x,sc.y,sc.w,sc.h);
    wx.dispatchPaintEvent(ex);
    }
  }

const std::shared_ptr<Widget::Ref>& Widget::selfReference() {
  if(selfRef==nullptr)
    selfRef = std::make_shared<Widget::Ref>(this);
  return selfRef;
  }

void Widget::setOwner(Widget *w) {
  if(ow==w)
    return;
  if(ow)
    ow->takeWidget(this);
  ow=w;
  }

Widget& Widget::implAddWidget(Widget *w) {
  if(w==nullptr)
    throw std::invalid_argument("null widget");
  if(w->checkFocus()) {
    while(true) {
      implClearFocus(this,&Additive::focus,&State::focus);
      auto root=implTrieRoot(this);
      if(root==nullptr || !root->checkFocus())
        break;
      }
    if(w->checkFocus())
      implAttachFocus();
    }

  wx.emplace_back(w);
  w->setOwner(this);
  if(w->checkFocus())
    state.focus = w;
  if(state.disable>0)
    implDisableSum(w,state.disable);
  lay->applyLayout();
  update();
  return *w;
  }

Widget *Widget::takeWidget(Widget *w) {
  if(state.focus==w) {
    auto wx = this;
    while(wx!=nullptr){
      wx->state.focus=nullptr;
      wx = wx->owner();
      }
    }
  if(state.disable>0)
    implDisableSum(w,-state.disable);
  const size_t id=lay->find(w);
  if(iterator!=nullptr)
    iterator->onDelete(id,w);
  return lay->takeWidget(id);
  }

Point Widget::mapToRoot(const Point &p) const {
  const Widget* w=this;
  Point ret=p;

  while(w!=nullptr){
    ret += w->pos();
    w   =  w->owner();
    }

  return ret;
  }

void Widget::setPosition(int x, int y) {
  if(wrect.x==x && wrect.y==y)
    return;

  wrect.x = x;
  wrect.y = y;
  update();
  }

void Widget::setPosition(const Point &pos) {
  setPosition(pos.x,pos.y);
  }

void Widget::setGeometry(const Rect &rect) {
  if(wrect==rect)
    return;
  bool resize=(wrect.w!=rect.w || wrect.h!=rect.h);
  wrect=rect;
  update();

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
  if(ow!=nullptr)
    ow->lay->applyLayout();
  }

void Widget::setSizeHint(const Size &s, const Margin &add) {
  setSizeHint(Size(s.w+add.xMargin(),s.h+add.yMargin()));
  }

void Widget::setSizePolicy(SizePolicyType hv) {
  setSizePolicy(hv,hv);
  }

void Widget::setSizePolicy(SizePolicyType h, SizePolicyType v) {
  if(szPolicy.typeH==h && szPolicy.typeV==v)
    return;
  szPolicy.typeH=h;
  szPolicy.typeV=v;

  if(ow!=nullptr)
    ow->lay->applyLayout();
  }

void Widget::setSizePolicy(const SizePolicy &sp) {
  if(szPolicy==sp)
    return;
  szPolicy=sp;
  if(ow!=nullptr)
    ow->lay->applyLayout();
  }

void Widget::setFocusPolicy(FocusPolicy f) {
  fcPolicy=f;
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

void Widget::setEnabled(bool e) {
  if(e!=wstate.disabled)
    return;

  if(!wstate.disabled) {
    wstate.disabled=true;
    implDisableSum(this,+1);
    } else {
    wstate.disabled=false;
    implDisableSum(this,-1);
    }
  update();
  }

bool Widget::isEnabled() const {
  return state.disable==0;
  }

bool Widget::isEnabledTo(const Widget *ancestor) const {
  const Widget* w = this;
  while( w ){
    if(w->wstate.disabled)
       return false;
    if( w==ancestor )
       return true;
    w = w->owner();
    }
  return true;
  }

void Widget::setFocus(bool b) {
  implSetFocus(&Additive::focus,&State::focus,b,nullptr);
  }

void Widget::implSetFocus(Widget* Additive::*add, bool State::*flag, bool value,const MouseEvent* parent) {
  if(wstate.*flag==value)
    return;

  Widget* previous=nullptr;
  Widget* next    =this;

  Widget* w=this;
  while(true) {
    if(w->state.*add!=nullptr || w->owner()==nullptr) {
      while(true) {
        auto mv=w->state.*add;
        w->state.*add=nullptr;
        if(mv==nullptr){
          w->wstate.*flag=false;
          previous = w;
          break;
          }
        w=mv;
        }

      w=this;
      while(w->owner()) {
        w->owner()->state.*add=w;
        w = w->owner();
        }
      next->wstate.*flag=true;

      if(parent!=nullptr)
        implExcFocus(Event::MouseEnter,previous,next,*parent); else
        implExcFocus(Event::MouseEnter,previous,next,MouseEvent());
      return;
      }
    w = w->owner();
    }
  }

void Widget::implClearFocus(Widget *wx,Widget* Additive::*add, bool State::*flag) {
  Widget* w=implTrieRoot(wx);
  while(w!=nullptr) {
    if(w->state.*add!=nullptr) {
      auto x = w->state.*add;
      w->state.*add=nullptr;
      w = x;
      continue;
      }
    if(w->wstate.*flag){
      FocusEvent e(false,Event::FocusReason::UnknownReason);
      w->wstate.*flag=false;
      w->focusEvent(e);
      }
    return;
    }
  }

Widget *Widget::implTrieRoot(Widget *w) {
  while(w->owner()!=nullptr)
    w = w->owner();
  return w;
  }

void Widget::implAttachFocus() {
  Widget* w = this, *prev=nullptr;
  while(w!=nullptr) {
    w->state.focus = prev;
    prev=w;
    w = w->owner();
    }
  }

void Widget::implExcFocus(Event::Type type,Widget *prev,Widget *next,const MouseEvent& parent) {
  if(prev!=nullptr){
    auto mpos = next->mapToRoot(parent.pos()) - prev->mapToRoot(Point());
    MouseEvent ex(mpos.x,
                  mpos.y,
                  parent.button,
                  parent.delta,
                  parent.mouseID,
                  Event::MouseLeave);
    prev->mouseLeaveEvent(ex);
    }

  MouseEvent ex(parent.x,
                parent.y,
                parent.button,
                parent.delta,
                parent.mouseID,
                type);
  next->mouseEnterEvent(ex);
  }

void Widget::update() {
  Widget* w=this;
  while(w!=nullptr){
    if(w->state.needToUpdate)
      return;
    w->state.needToUpdate=true;
    w = w->owner();
    }
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

void Widget::mouseDragEvent(MouseEvent &e) {
  e.ignore();
  }

void Widget::mouseWheelEvent(MouseEvent &e) {
  e.ignore();
  }

void Widget::keyDownEvent(KeyEvent &e) {
  e.ignore();
  }

void Widget::keyUpEvent(KeyEvent &e) {
  e.ignore();
  }

void Widget::mouseEnterEvent(MouseEvent &e) {
  e.ignore();
  }

void Widget::mouseLeaveEvent(MouseEvent &e) {
  e.ignore();
  }

void Widget::focusEvent(FocusEvent &e) {
  e.ignore();
  }
