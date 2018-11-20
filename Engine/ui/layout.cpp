#include "layout.h"

#include <Tempest/Widget>

using namespace Tempest;

Layout::Layout() {
  }

size_t Layout::count() const {
  return w ? w->wx.size() : 0;
  }

Widget *Layout::at(size_t i) {
  return w->wx[i];
  }

Widget *Layout::takeWidget(Widget *wx) {
  for(size_t i=0;i<w->wx.size();++i)
    if(w->wx[i]==wx) {
      w->wx.erase(w->wx.begin()+int(i));
      wx->ow=nullptr;
      applyLayout();
      return wx;
      }
  return nullptr;
  }

void Layout::bind(Widget *wx) {
  w = wx;
  applyLayout();
  }

void LinearLayout::applyLayout(Widget &w, Orienration ori) {
  int    area=w.w();
  int    h   =w.h();
  int    hint=0;
  bool   exp=false;
  size_t count=w.widgetsCount();
  for(size_t i=0;i<count;++i){
    hint+=w.widget(i).sizeHint().w;
    }

  if(exp) {
    int x=0;
    for(size_t i=0;i<count;++i){
      Widget& wx = w.widget(i);
      Size    sz = wx.sizeHint();

      wx.setGeometry(x,0,sz.w,h);
      x+=sz.w;
      }
    //todo
    } else {
    int x=0;
    for(size_t i=0;i<count;++i){
      Widget& wx = w.widget(i);
      Size    sz = wx.sizeHint();

      wx.setGeometry(x,0,sz.w,h);
      x+=sz.w;
      }
    }
  }
