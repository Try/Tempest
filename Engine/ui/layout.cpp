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

size_t Layout::find(Widget *wx) const {
  for(size_t i=0;i<w->wx.size();++i)
    if(w->wx[i]==wx)
      return i;
  return size_t(-1);
  }

Widget *Layout::takeWidget(size_t i) {
  if(i<w->wx.size()){
    Widget* wx=w->wx[i];
    w->wx.erase(w->wx.begin()+int(i));
    wx->ow=nullptr;
    applyLayout();
    return wx;
    }
  return nullptr;
  }

Widget *Layout::takeWidget(Widget *wx) {
  return takeWidget(find(wx));
  }

void Layout::bind(Widget *wx) {
  w = wx;
  applyLayout();
  }

void LinearLayout::applyLayout(Widget &w, Orientation ori) {
  if(ori==Horizontal)
    implApplyLayout<true>(w); else
    implApplyLayout<false>(w);
  }

template<bool hor>
void LinearLayout::implApplyLayout(Widget &w) {
  int    fixSize  = 0;
  int    prefSize = 0;
  int    expSize  = 0;

  size_t count = w.widgetsCount();
  int    pref  = 0;
  int    exp   = 0;

  size_t visCount = 0;
  for(size_t i=0;i<count;++i){
    if(w.widget(i).isVisible())
      visCount++;
    }

  for(size_t i=0;i<count;++i){
    Widget& wx = w.widget(i);
    if(!wx.isVisible())
      continue;

    switch(getType<hor>(wx.sizePolicy())) {
      case Fixed: {
        int max  = getW<hor>(wx.maxSize());
        int min  = getW<hor>(wx.minSize());
        int hint = getW<hor>(wx.sizeHint());
        if(hint>max)
          hint=max;
        if(hint<min)
          hint=min;
        fixSize+=hint;
        break;
        }
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
  freeSpace -= (visCount==0 ? 0 : (int(visCount)-1)*w.spacing());
  if(freeSpace<0)
    freeSpace=0;
  implApplyLayout<hor>(w,count,visCount,exp>0,((exp>0) ? expSize : prefSize),freeSpace,(exp>0) ? exp : pref);
  }

template<bool hor>
void LinearLayout::implApplyLayout(Widget &w, size_t count, size_t visCount,
                                   bool exp, int sum, int free, int expCount) {
  auto   client=w.clientRect();
  const SizePolicyType tExp=(exp ? Expanding : Preferred);

  int c      =hor ? client.x : client.y;
  int h      =getW<!hor>(client);
  int spacing=w.spacing();

  if(expCount>0){
    sum+=free;
    free=0;
    }

  for(size_t wi=0,i=0;wi<count;++wi){
    Widget& wx = w.widget(wi);
    if(!wx.isVisible())
      continue;

    auto&   sp = wx.sizePolicy();
    Size    sz = wx.sizeHint();
    const int freeSpace=free/int(visCount+1-i);
    c+=freeSpace;
    free-=freeSpace;

    int ww=0;
    int wh=getW<!hor>(sz);

    if(getType<hor>(sp)==tExp) {
      ww   = clamp(getW<hor>(wx.minSize()),sum/expCount,getW<hor>(wx.maxSize()));
      sum -= ww;
      expCount--;
      } else {
      ww   = clamp(getW<hor>(wx.minSize()),getW<hor> (sz),getW<hor>(wx.maxSize()));
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
    ++i;
    }
  }
