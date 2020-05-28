#include "listview.h"

using namespace Tempest;

ListView::ListView(Orientation ori)
  : sc(ori) {
  sc.scrollAfterEndV(true);
  sc.setMargins(Margin(0));
  addWidget(&sc);
  setSizePolicy(Preferred);
  Widget::setLayout(Vertical);
  }

ListView::~ListView() {
  removeDelegate();
  }

void ListView::setDelegate(ListDelegate* d) {
  removeDelegate();

  delegate.reset(d);
  delegate->onItemSelected.bind(&onItemSelected,&Tempest::Signal<void(size_t)>::operator());
  delegate->invalidateView.bind(this,&ListView::invalidateView);
  delegate->updateView    .bind(this,&ListView::updateView    );

  updateView();
  }

void ListView::removeDelegate() {
  if(!delegate)
    return;
  sc.centralWidget().removeAllWidgets();

  delegate->onItemSelected.ubind(&onItemSelected,&Tempest::Signal<void(size_t)>::operator());
  delegate->invalidateView.ubind(this,&ListView::invalidateView);
  delegate->updateView    .ubind(this,&ListView::updateView    );
  }

void ListView::setLayout(Orientation ori) {
  sc.setLayout(ori);
  }

void ListView::setDefaultItemRole(ListDelegate::Role role) {
  if(role==defaultRole)
    return;
  defaultRole = role;
  invalidateView();
  }

void ListView::invalidateView(){
  auto&  w      = sc.centralWidget();
  while(w.widgetsCount()>0) {
    size_t i=w.widgetsCount()-1;
    auto wx = w.takeWidget(&w.widget(i));
    delegate->removeView(wx,i);
    }

  updateView();
  onItemListChanged();
  }

void ListView::updateView() {
  auto&  w      = sc.centralWidget();
  size_t cnt    = delegate->size();
  size_t wcount = w.widgetsCount();

  // remove extra widgets
  while(w.widgetsCount()>cnt) {
    size_t i=w.widgetsCount()-1;
    auto wx = w.takeWidget(&w.widget(i));
    delegate->removeView(wx,i);
    }

  // update old-ones
  for(size_t i=0;i<cnt && i<wcount;++i) {
    auto wx = w.takeWidget(&w.widget(i));
    wx = delegate->update(wx,i);
    w.addWidget(wx,i);
    }

  // new widgets
  for(size_t i=wcount;i<cnt;++i){
    w.addWidget(delegate->createView(i,defaultRole));
    }
  setSizeHint(sc.sizeHint());
  onItemListChanged();
  }
