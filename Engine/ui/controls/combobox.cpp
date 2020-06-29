#include "combobox.h"

#include <Tempest/Application>
#include <Tempest/ListView>
#include <Tempest/Painter>

using namespace Tempest;

struct ComboBox::Overlay:UiOverlay {
  Overlay(ComboBox* owner):owner(owner) {
    }
  ~Overlay() {
    owner->overlay = nullptr;
    }

  void mouseDownEvent(MouseEvent&) override {
    delete this;
    }

  ComboBox* owner;
  };


struct ComboBox::ProxyDelegate : ListDelegate {
  ProxyDelegate(ListDelegate& owner):owner(owner){}

  size_t  size() const override { return owner.size(); }

  Widget* createView(size_t position, Role role) override { return owner.createView(position,role); }
  Widget* createView(size_t position           ) override { return owner.createView(position,R_ListBoxItem); }
  void    removeView(Widget* w, size_t position) override { return owner.removeView(w,position); }
  Widget* update    (Widget* w, size_t position) override { return owner.update(w,position); }

  ListDelegate& owner;
  };


ComboBox::ComboBox() {
  setMargins(0);
  auto& m = style().metrics();
  setSizeHint(Size(m.buttonSize,m.buttonSize));
  setSizePolicy(Preferred,Fixed);
  setLayout(Horizontal);
  }

ComboBox::~ComboBox() {
  delete overlay;
  }

void ComboBox::proxyOnItemSelected(size_t /*id*/) {
  if(overlay==nullptr) {
    openMenu();
    } else {
    closeMenu();
    }
  }

void ComboBox::setDelegate(ListDelegate* d) {
  removeDelegate();

  delegate.reset(d);
  delegate->onItemSelected.bind(this,&ComboBox::proxyOnItemSelected);
  delegate->invalidateView.bind(this,&ComboBox::invalidateView     );
  delegate->updateView    .bind(this,&ComboBox::updateView         );

  updateView();
  }

void ComboBox::removeDelegate() {
  if(!delegate)
    return;
  this->removeAllWidgets();

  delegate->onItemSelected.ubind(this,&ComboBox::proxyOnItemSelected);
  delegate->invalidateView.ubind(this,&ComboBox::invalidateView     );
  delegate->updateView    .ubind(this,&ComboBox::updateView         );
  }

void ComboBox::setCurrentIndex(size_t id) {
  if(selectedId==id)
    return;
  selectedId = id;
  onSelectionChanged(id);
  updateView();
  }

size_t ComboBox::currentIndex() const {
  return selectedId;
  }

void ComboBox::invalidateView() {
  if(widgetsCount()>0) {
    auto w = this->takeWidget(&widget(0));
    delegate->removeView(w,selectedId);
    }
  addWidget(delegate->createView(selectedId,ListDelegate::R_ListBoxView));
  if(proxyDelegate!=nullptr)
    proxyDelegate->invalidateView();
  }

void ComboBox::updateView() {
  if(widgetsCount()>0) {
    auto w = this->takeWidget(&widget(0));
    w = delegate->update(w,selectedId);
    addWidget(w);
    } else {
    addWidget(delegate->createView(selectedId,ListDelegate::R_ListBoxView));
    }
  if(proxyDelegate!=nullptr)
    proxyDelegate->updateView();
  }

void ComboBox::openMenu() {
  overlay = new Overlay(this);
  auto list = createDropList();
  overlay->addWidget(list);

  list->setPosition(this->mapToRoot(Point(0,h())));
  list->resize(std::max(w(),list->w()),list->h());

  SystemApi::addOverlay(overlay);
  }

void ComboBox::closeMenu() {
  delete overlay;
  overlay = nullptr;
  proxyDelegate = nullptr;
  }

void ComboBox::applyItemSelection(size_t id) {
  if(id==selectedId)
    return;
  setCurrentIndex(id);
  onItemSelected(id);
  onSelectionChanged(id);
  }

Widget* ComboBox::createDropList() {
  ListView* list = new ListView();
  proxyDelegate = new ProxyDelegate(*delegate);
  list->setDelegate(proxyDelegate);
  list->onItemSelected.bind(this,&ComboBox::applyItemSelection);

  auto szh = list->centralWidget().sizeHint();
  list->resize(szh);
  return list;
  }
