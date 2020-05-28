#include "scrollwidget.h"

#include <Tempest/Platform>
#include <Tempest/Layout>

using namespace Tempest;

struct ScrollWidget::BoxLayout: public Tempest::LinearLayout {
  BoxLayout(ScrollWidget* sc,Orientation ori ):LinearLayout(ori), sc(sc){}

  void applyLayout() {
    sc->complexLayout();
    }

  void commitLayout() {
    LinearLayout::applyLayout();
    }

  Size wrapContent(Widget& ow, Orientation orient, bool inParent) {
    const Widget* root=inParent ? sc : nullptr;
    int sw=0,sh=0;

    int vcount = 0;
    for(size_t i=0;i<ow.widgetsCount();i++) {
      auto& wx = ow.widget(i);
      if( !wx.isVisible() )
        continue;
      vcount++;
      Size s   = wx.sizeHint();
      Size min = wx.minSize();
      s.w = std::max(s.w,min.w);
      s.h = std::max(s.h,min.h);

      if(orient==Horizontal){
        sw += s.w;
        sh = std::max(sh,s.h);
        } else {
        sw = std::max(sw,s.w);
        sh += s.h;
        }
      }

    if(orient==Horizontal) {
      if(vcount>0)
        sw += (vcount-1)*ow.spacing();
      if(root)
        sh=std::max(sh,root->h());
      } else {
      if(vcount>0)
        sh += (vcount-1)*ow.spacing();
      if(root)
        sw=std::max(sw,root->w());
      }

    const Margin& m = ow.margins();
    if(orient==Horizontal)
      sw += m.xMargin(); else
      sh += m.yMargin();
    return Size(sw,sh);
    }

  ScrollWidget* sc = nullptr;
  };

struct ScrollWidget::ProxyLayout : public Tempest::Layout {
  ProxyLayout(ScrollWidget* sc):sc(sc){}

  void applyLayout() {
    sc->complexLayout();
    }

  ScrollWidget* sc = nullptr;
  };


ScrollWidget::ScrollWidget()
  :ScrollWidget(Vertical) {
  }

ScrollWidget::ScrollWidget(Orientation ori)
  : sbH(Horizontal), sbV(Vertical), vert(AsNeed), hor(AsNeed)  {
  layoutBusy=true;

  const Style::UIIntefaceCategory cat=style().idiom().category;
  if( cat==Style::UIIntefacePhone || cat==Style::UIIntefacePC ){
    vert=AlwaysOff;
    hor =AlwaysOff;
    }
  setSizePolicy(Preferred);

  sbH.onValueChanged.bind(this, static_cast<void(ScrollWidget::*)(int)>(&ScrollWidget::scrollH));
  sbV.onValueChanged.bind(this, static_cast<void(ScrollWidget::*)(int)>(&ScrollWidget::scrollV));

  setHscrollViewMode(this->hor );
  setVscrollViewMode(this->vert);

  helper.addWidget(&cen);
  addWidget(&helper);
  addWidget(&sbH);
  addWidget(&sbV);

  cenLay = new BoxLayout(this,ori);
  cen.setLayout(cenLay);
  helper. setLayout(new Tempest::Layout());
  Widget::setLayout(new ProxyLayout (this));

  layoutBusy=false;
  }

ScrollWidget::~ScrollWidget() {
  helper.takeWidget(&cen);
  takeWidget(&helper);
  takeWidget(&sbH);
  takeWidget(&sbV);
  }

bool ScrollWidget::updateScrolls(Orientation orient,bool noRetry) {
  using std::max;
  using std::min;

  const Margin& m     = margins();
  const Widget* first = findFirst();
  const Widget* last  = findLast();

  Size content = cenLay->wrapContent(cen,orient,false);

  bool needScH = (hor ==AlwaysOn || content.w>helper.w());
  bool needScV = (vert==AlwaysOn || content.h>helper.h());
  bool hasScH  = needScH && (hor !=AlwaysOff);
  bool hasScV  = needScV && (vert!=AlwaysOff);

  emplace(helper,
          hasScH ? &sbH : nullptr,
          hasScV ? &sbV : nullptr,
          Rect(m.left,m.top,w()-m.xMargin(),h()-m.yMargin()));

  const Size hsize=helper.size();
  if( !noRetry ) {
    if(hasScH != (hor ==AlwaysOn || content.w>hsize.w) && (hor!=AlwaysOff))
      return false;
    if(hasScV != (vert==AlwaysOn || content.h>hsize.h) && (vert!=AlwaysOff))
      return false;
    }

  if( !needScH && !needScV ) {
    sbH.setValue(0);
    sbV.setValue(0);
    cen.setPosition(0,0);
    } else
  if( !needScH ) {
    sbH.setValue(0);
    cen.setPosition(0,cen.y());
    } else
  if( !needScV ) {
    sbV.setValue(0);
    cen.setPosition(cen.x(),0);
    }
  sbH.setVisible(hasScH);
  sbV.setVisible(hasScV);
  cen.applyLayout();

  if(orient==Vertical) {
    int min=0;
    int max=std::max(content.h-hsize.h,0);

    if(scBeforeBeginV && first)
      min-=std::max(0,hsize.h-first->h());
    if(scAfterEndV && last)
      max+=std::max(0,hsize.h-last->h());
    sbV.setRange(min,max);
    sbH.setRange(0,cen.w());
    } else {
    int min=0;
    int max=std::max(content.w-hsize.w,0);

    if(scBeforeBeginH && first)
      min-=std::max(0,hsize.w-first->w());
    if(scAfterEndV && last)
      max+=std::max(0,hsize.w-last->w());
    sbH.setRange(min,max);
    sbV.setRange(cen.h(),0);
    }

  if(sbH.range()>0) {
    const double percent=hsize.w/double(sbH.range()+hsize.w);
    sbH.setCentralButtonSize(int(sbH.centralAreaSize()*percent));
    }
  if(sbV.range()>0) {
    const double percent=hsize.h/double(sbV.range()+hsize.h);
    sbV.setCentralButtonSize(int(sbV.centralAreaSize()*percent));
    }

  return true;
  }

void ScrollWidget::emplace(Widget &cen, Widget *scH, Widget *scV, const Rect& place) {
  int sp = spacing();
  int dx = scV==nullptr ? 0 : (scV->sizeHint().w);
  int dy = scH==nullptr ? 0 : (scH->sizeHint().h);

  if(scH)
    scH->setGeometry(place.x,place.y+place.h-dy,place.w-dx,dy);
  if(scV)
    scV->setGeometry(place.x+place.w-dx,place.y,dx,place.h-dy);

  if(dx>0)
    dx+=sp;
  if(dy>0)
    dy+=sp;
  cen.setGeometry(place.x,place.y,std::max(0,place.w-dx),std::max(0,place.h-dy));
  }

Widget *ScrollWidget::findFirst() {
  size_t sz = cen.widgetsCount();
  for(size_t i=0;i<sz;++i){
    if(cen.widget(i).isVisible())
      return &cen.widget(i);
    }
  return nullptr;
  }

Widget *ScrollWidget::findLast() {
  for(size_t i=cen.widgetsCount();i>0;){
    i--;
    if(cen.widget(i).isVisible())
      return &cen.widget(i);
    }
  return nullptr;
  }

Tempest::Widget &ScrollWidget::centralWidget() {
  return cen;
  }

void ScrollWidget::setLayout(Orientation ori) {
  layoutBusy=true;
  cenLay = new BoxLayout(this,ori);
  cen.setLayout(cenLay);
  layoutBusy=false;
  applyLayout();
  }

void ScrollWidget::hideScrollBars() {
  setScrollBarsVisible(false,false);
  }

void ScrollWidget::setScrollBarsVisible(bool vh, bool vv) {
  if(vh==sbH.isVisible() && vv==sbV.isVisible())
    return;

  sbH.setVisible(vh);
  sbV.setVisible(vv);
  }

void ScrollWidget::setVscrollViewMode(ScrollViewMode mode) {
  vert = mode;
  applyLayout();
  }

void ScrollWidget::setHscrollViewMode(ScrollViewMode mode) {
  hor = mode;
  applyLayout();
  }

void ScrollWidget::scrollAfterEndH(bool s) {
  scAfterEndH = s;
  applyLayout();
  }

bool ScrollWidget::hasScrollAfterEndH() const {
  return scAfterEndH;
  }

void ScrollWidget::scrollBeforeBeginH(bool s) {
  scBeforeBeginH = s;
  applyLayout();
  }

bool ScrollWidget::hasScrollBeforeBeginH() const {
  return scBeforeBeginH;
  }

void ScrollWidget::scrollAfterEndV(bool s) {
  scAfterEndV = s;
  applyLayout();
  }

bool ScrollWidget::hasScrollAfterEndV() const {
  return scAfterEndV;
  }

void ScrollWidget::scrollBeforeBeginV(bool s) {
  scBeforeBeginV = s;
  applyLayout();
  }

bool ScrollWidget::hasScrollBeforeBeginV() const {
  return scBeforeBeginV;
  }

void ScrollWidget::mouseWheelEvent(Tempest::MouseEvent &e) {
  if(!isEnabled())
    return;

  if( !rect().contains(e.x+x(), e.y+y()) || !sbV.isVisible() ) {
    e.ignore();
    return;
    }

  if(e.delta>0)
    sbV.setValue(sbV.value() - sbV.largeStep()); else
  if(e.delta<0)
    sbV.setValue(sbV.value() + sbV.largeStep());
  }

void ScrollWidget::mouseMoveEvent(Tempest::MouseEvent &e) {
  e.ignore();
  }

void ScrollWidget::scrollH( int v ) {
  sbH.setValue( v );
  cen.setPosition(-sbH.value(), cen.y());
  }

void ScrollWidget::scrollV(int v) {
  sbV.setValue( v );
  cen.setPosition(cen.x(), -sbV.value());
  }

int ScrollWidget::scrollH() const {
  return -cen.x();
  }

int ScrollWidget::scrollV() const {
  return -cen.y();
  }

void ScrollWidget::complexLayout() {
  if(layoutBusy)
    return;

  Orientation orient = cenLay->orientation();

  layoutBusy=true;
  helper.setGeometry(0,0,w(),h());
  wrapContent();
  cenLay->commitLayout();

  static const int tryCound=3;
  for(int i=1;i<=tryCound;++i)
    if(updateScrolls(orient,i==tryCound))
      break;
  layoutBusy=false;
  }

void ScrollWidget::wrapContent() {
  SizePolicy spolicy;
  auto wrap = contentAreaSize();
  spolicy.maxSize=wrap;
  spolicy.minSize=wrap;
  if(cenLay->orientation()==Horizontal) {
    spolicy.typeH  =Fixed;
    spolicy.typeV  =Preferred;
    } else {
    spolicy.typeH  =Preferred;
    spolicy.typeV  =Fixed;
    }

  setSizeHint(wrap);
  cen.resize(wrap);
  }

Size ScrollWidget::contentAreaSize() {
  return cenLay->wrapContent(cen,cenLay->orientation(),true);
  }
