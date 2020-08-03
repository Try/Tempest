#include "combobox.h"

#include <Tempest/Application>
#include <Tempest/ListView>
#include <Tempest/Painter>
#include <Tempest/Panel>

using namespace Tempest;

struct ComboBox::Overlay:UiOverlay {
  Overlay(ComboBox* owner):owner(owner) {}
  ~Overlay() {
    owner->overlay = nullptr;
    }

  void mouseDownEvent(MouseEvent&) override {
    owner->closeMenu();
    }

  ComboBox* owner;
  };


struct ComboBox::DropPanel:Tempest::Panel {
  void paintEvent(Tempest::PaintEvent& e) {
    Tempest::Painter p(e);
    style().draw(p,this,Style::E_MenuBackground,state(),Rect(0,0,w(),h()),Style::Extra(*this));
    }
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


template<class T>
struct ComboBox::DefaultDelegate : ArrayListDelegate<T,Button> {
  DefaultDelegate(std::vector<std::string>&& items):ArrayListDelegate<T,Button>(this->items), items(items){}
  size_t  size() const override { return items.size(); }

  Widget* createView(size_t i, ListDelegate::Role role) override {
    auto* w = ArrayListDelegate<T,Button>::createView(i,role);
    if(auto b = dynamic_cast<Button*>(w))
      b->setButtonType(Button::T_FlatButton);
    return w;
    }

  Widget* createView(size_t i) override {
    auto* w = ArrayListDelegate<T,Button>::createView(i);
    if(auto b = dynamic_cast<Button*>(w))
      b->setButtonType(Button::T_FlatButton);
    return w;
    }

  Widget* update    (Widget* w, size_t i) override {
    if(auto b = dynamic_cast<Button*>(w)) {
      b->setText(items[i]);
      return b;
      }
    return ArrayListDelegate<T,Button>::update(w,i);
    }

  std::vector<std::string> items;
  };


ComboBox::ComboBox() {
  setMargins(0);
  auto& m = style().metrics();
  setSizeHint(Size(m.buttonSize,m.buttonSize));
  setSizePolicy(Preferred,Fixed);
  setLayout(Horizontal);
  }

ComboBox::~ComboBox() {
  closeMenu();
  }

void ComboBox::setItems(const std::vector<std::string>& items) {
  auto i = items;
  setDelegate(new DefaultDelegate<std::string>(std::move(i)));
  }

void ComboBox::proxyOnItemSelected(size_t id, Widget*) {
  if(overlay==nullptr) {
    openMenu();
    } else {
    closeMenu();
    applyItemSelection(id);
    }
  }

void ComboBox::implSetDelegate(ListDelegate* d) {
  removeDelegate();

  delegate.reset(d);
  delegate->onItemViewSelected.bind(this,&ComboBox::proxyOnItemSelected);
  delegate->invalidateView    .bind(this,&ComboBox::invalidateView     );
  delegate->updateView        .bind(this,&ComboBox::updateView         );

  updateView();
  }

void ComboBox::removeDelegate() {
  if(!delegate)
    return;
  this->removeAllWidgets();

  delegate->onItemViewSelected.ubind(this,&ComboBox::proxyOnItemSelected);
  delegate->invalidateView    .ubind(this,&ComboBox::invalidateView     );
  delegate->updateView        .ubind(this,&ComboBox::updateView         );
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
  {
  auto w = delegate->createView(selectedId,ListDelegate::R_ListBoxView);
  setSizeHint(w->sizeHint());
  addWidget(w);
  }

  if(proxyDelegate!=nullptr)
    proxyDelegate->invalidateView();
  }

void ComboBox::updateView() {
  if(widgetsCount()>0) {
    auto w = this->takeWidget(&widget(0));
    w = delegate->update(w,selectedId);
    setSizeHint(w->sizeHint());
    addWidget(w);
    } else {
    auto w = delegate->createView(selectedId,ListDelegate::R_ListBoxView);
    setSizeHint(w->sizeHint());
    addWidget(w);
    }
  if(proxyDelegate!=nullptr)
    proxyDelegate->updateView();
  }

void ComboBox::paintEvent(PaintEvent& e) {
  Tempest::Painter p(e);
  style().draw(p,this, Style::E_Background,state(),Rect(0,0,w(),h()),Style::Extra(*this));
  }

void ComboBox::mouseWheelEvent(MouseEvent& e) {
  auto size = delegate->size();
  if(size==0 || e.delta==0)
    return;
  size_t ci = currentIndex();
  int d  = (e.delta<0 ? 1 : -1);
  if(d<0 && ci==0)
    return;
  if(ci+d>=size)
    return;
  applyItemSelection(ci+d);
  }

void ComboBox::openMenu() {
  overlay = new Overlay(this);
  auto list = createDropList();
  overlay->addWidget(list);

  list->setPosition(this->mapToRoot(Point(0,0)));
  list->resize(std::max(w(),list->w()),list->h());

  SystemApi::addOverlay(overlay);
  while(overlay!=nullptr && Application::isRunning()) {
    Application::processEvents();
    }
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
  Panel* p = new DropPanel();
  p->setLayout(Horizontal);
  p->setMargins(0);

  ListView* list = new ListView();
  proxyDelegate = new ProxyDelegate(*delegate);
  list->setDelegate(proxyDelegate);
  list->onItemSelected.bind(this,&ComboBox::applyItemSelection);

  auto szh = list->centralWidget().sizeHint();
  p->resize(szh);
  p->addWidget(list);
  return p;
  }
