#include "layout.h"

#include <Tempest/Widget>

using namespace Tempest;

static int clamp(int min,int v,int max){
  if(v<min)
    return min;
  if(v>max)
    return max;
  return v;
  }

template<bool v>
inline int getW(const Size& w){
  return w.w;
  }

template<>
inline int getW<false>(const Size& w){
  return w.h;
  }

template<bool v>
inline int getW(const Rect& w){
  return w.w;
  }

template<>
inline int getW<false>(const Rect& w){
  return w.h;
  }

template<bool v>
inline int getType(const SizePolicy& sp){
  return sp.typeH;
  }

template<>
inline int getType<false>(const SizePolicy& w){
  return w.typeV;
  }


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
  if(ori==Horizontal)
    implApplyLayout<true>(w); else
    implApplyLayout<false>(w);
  }

template<bool hor>
void LinearLayout::implApplyLayout(Widget &w) {
  int    fixSize =0;
  int    prefSize=0;
  int    expSize =0;

  size_t count=w.widgetsCount();
  int    pref =0;
  int    exp  =0;

  for(size_t i=0;i<count;++i){
    Widget& wx = w.widget(i);

    switch(getType<hor>(wx.sizePolicy())) {
      case Fixed:
        fixSize  += getW<hor>(wx.sizeHint());
        break;
      case Preferred:
        prefSize += getW<hor>(wx.sizeHint());
        pref++;
        break;
      case Expanding:
        expSize  += getW<hor>(wx.sizeHint());
        exp++;
        break;
      }
    }

  int freeSpace = getW<hor>(w.size())-(hor ? w.margins().xMargin() : w.margins().yMargin());
  freeSpace -= (fixSize+prefSize+expSize);
  freeSpace -= (count==0 ? 0 : (int(count)-1)*w.spacing());
  if(freeSpace<0)
    freeSpace=0;
  implApplyLayout<hor>(w,count,exp>0,((exp>0) ? expSize : prefSize),freeSpace,(exp>0) ? exp : pref);
  }

template<bool hor>
void LinearLayout::implApplyLayout(Widget &w, size_t count, bool exp, int sum, int free, int expCount) {
  auto   client=w.clentRet();
  const SizePolicyType tExp=(exp ? Expanding : Preferred);

  int c      =hor ? client.x : client.y;
  int h      =getW<!hor>(client);
  int spacing=w.spacing();

  if(expCount>0){
    sum+=free;
    free=0;
    }

  for(size_t i=0;i<count;++i){
    Widget& wx = w.widget(i);
    auto&   sp = wx.sizePolicy();
    Size    sz = wx.sizeHint();
    const int freeSpace=free/int(count+1-i);
    c+=freeSpace;
    free-=freeSpace;

    int ww=getW<hor> (sz);
    int wh=getW<!hor>(sz);

    if(getType<hor>(sp)==tExp) {
      ww   = clamp(getW<hor>(wx.minSize()),sum/expCount,getW<hor>(wx.maxSize()));
      sum -= ww;
      expCount--;
      }

    wh = clamp(getW<!hor>(wx.minSize()), wh, getW<!hor>(wx.maxSize()));
    if(getType<!hor>(sp)!=Fixed)
      wh = h; else
      wh = std::min(h,wh);

    if(hor) {
      wx.setGeometry(c,client.y+(h-wh)/2,ww,wh);
      c+=ww;
      } else {
      wx.setGeometry(client.x+(h-wh)/2,c,wh,ww);
      c+=ww;
      }

    c+=spacing;
    }
  }
