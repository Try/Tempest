#include "menu.h"

#include <Tempest/Layout>
#include <Tempest/Panel>
#include <Tempest/ListView>
#include <Tempest/ScrollWidget>
#include <Tempest/Painter>

using namespace Tempest;

struct Menu::Delegate : public ListDelegate {
    Delegate( Menu& owner, const std::vector<Menu::Item>& items )
      :owner(owner), items(items){}

    size_t  size() const{ return items.size(); }
    Widget* createView(size_t position){
      return owner.createItem(items[position]);
      }
    void    removeView(Widget* w, size_t /*position*/){
      delete w;
      }

    Menu&                          owner;
    const std::vector<Menu::Item>& items;
  };

Menu::Overlay::Overlay(Menu* menu, Widget* owner)
  :owner(owner), menu(menu) {
  }

Menu::Overlay::~Overlay() {
  menu->overlay = nullptr;
  }

void Menu::Overlay::mouseDownEvent(MouseEvent &e) {
  const Point pt0 = mapToRoot(e.pos());
  const Point pt1 = owner->mapToRoot(Point());
  const Point pt = pt0-pt1;
  if(!(pt.x>=0 && pt.y>=0 && pt.x<owner->w() && pt.y<owner->h()))
    e.ignore(); else
    e.accept();
  delete this;
  }

void Menu::Overlay::mouseMoveEvent(MouseEvent& e) {
  e.accept();
  }

void Menu::Overlay::resizeEvent(SizeEvent&) {
  invalidatePos();
  }

void Menu::Overlay::invalidatePos() {
  Widget* parent = owner==nullptr ? this : owner;
  for(auto& i:menu->panels) {
    if(i.widget==nullptr)
      continue;
    auto pos = parent->mapToRoot(i.pos);
    i.widget->setPosition(pos);
    // i.widget->resize(i.widget->sizeHint());
    i.widget->resize(100,100);
    menu->setupMenuPanel(*i.widget);
    parent = i.widget;
    }
  }


Menu::Menu(Declarator&& decl): decl(std::move(decl)){
  }

Menu::~Menu() {
  implClose();
  }

int Menu::exec(Widget &owner) {
  return exec(owner,Tempest::Point(0,owner.h()));
  }

int Menu::exec(Widget& owner, const Point &pos, bool alignWidthToOwner) {
  if(overlay!=nullptr)
    implClose();

  running = true;
  overlay = new Overlay(this,&owner);
  SystemApi::addOverlay(overlay);

  decl.level = 0;
  initMenuLevels(decl);

  Widget *box = createDropList(owner,alignWidthToOwner,decl.items);
  if(box==nullptr) {
    implClose();
    return -1;
    }
  panels.resize(1);
  panels[0].widget = box;
  panels[0].pos    = pos;
  overlay->addWidget( box );
  box->setFocus(true);

  while(overlay!=nullptr && running && Application::isRunning()) {
    Application::processEvents();
    }
  return 0;
  }

void Menu::close() {
  running = false;
  }

void Menu::implClose() {
  if(overlay!=nullptr){
    delete overlay;
    overlay = nullptr;
    }
  }

Widget *Menu::createDropList( Widget& owner,
                              bool alignWidthToOwner,
                              const std::vector<Item>& items ) {
  Widget* list = createItems(items);

  Panel *box = new MenuPanel();
  box->addWidget(list);
  box->setLayout(Tempest::Horizontal);

  int wx = list->minSize().w+box->margins().xMargin();
  if(alignWidthToOwner) {
    wx = std::max(owner.w(),wx);
    }
  box->resize(wx, list->h()+box->margins().yMargin());
  return box;
  }

Widget *Menu::createItems(const std::vector<Item>& items) {
  ListView* list = new ListView(Vertical);
  list->setDelegate(new Delegate(*this,items));
  return list;
  }

Widget *Menu::createItem(const Menu::Item &decl) {
  ItemButton* b = new ItemButton(decl.items);

  b->setText(decl.text);
  b->onClick.bind(&decl.activated,&Signal<void()>::operator());
  b->onClick.bind(this,&Menu::close);
  b->setIcon(decl.icon);
  b->onMouseEnter.bind(this,&Menu::openSubMenu);

  return b;
  }

void Menu::openSubMenu(const Declarator &decl, Widget &owner) {
  if(panels.size()<=decl.level)
    panels.resize(decl.level+1);

  for(size_t i=decl.level;i<panels.size(); ++i){
    if(panels[i].widget!=nullptr) {
      delete panels[i].widget;
      panels[i].widget = nullptr;
      }
    }

  if(!overlay || overlay->owner==nullptr || decl.items.size()==0)
    return;

  Widget *box = createDropList(*overlay->owner,false,decl.items);
  if(!box)
    return;
  box->setEnabled(owner.isEnabled());
  Widget* root = decl.level>0 ? panels[decl.level-1].widget : nullptr;

  panels[decl.level].widget = box;
  if(root!=nullptr)
    panels[decl.level].pos = Point(root->w(),owner.y()); else
    panels[decl.level].pos = Point(owner.w(),0);
  overlay->addWidget( box );
  box->setFocus(true);
  overlay->invalidatePos();
  }

void Menu::setupMenuPanel(Widget& box) {
  if(box.w()>overlay->w())
    box.resize(overlay->w(), box.h());

  if( box.h() > overlay->h() )
    box.resize( box.w(), overlay->h());

  if(box.y()+box.h() > overlay->h())
    box.setPosition(box.x(), overlay->h()-box.h());
  if(box.x()+box.w() > overlay->w())
    box.setPosition(overlay->w()-box.w(), box.y());
  if(box.x()<0)
    box.setPosition(0, box.y());
  if(box.y()<0)
    box.setPosition(box.x(), 0);

  box.setFocus(true);
  }

void Menu::initMenuLevels(Menu::Declarator &decl) {
  for(Item& i:decl.items){
    i.items.level = decl.level+1;
    initMenuLevels(i.items);
    }
  }

void Menu::assign(std::string &s, const char *ch) {
  s = ch;
  }

void Menu::assign(std::string& s, const char16_t *ch) {
  s = TextCodec::toUtf8(ch);
  }

void Menu::assign(std::string &s, const std::string &ch) {
  s = ch;
  }

void Menu::assign(std::string &s, const std::u16string &ch) {
  s = TextCodec::toUtf8(ch);
  }


Menu::ItemButton::ItemButton(const Declarator &item):item(item){
  }

void Menu::ItemButton::mouseEnterEvent(MouseEvent &) {
  onMouseEnter(item,*this);
  }

void Menu::ItemButton::paintEvent(PaintEvent &e) {
  Tempest::Painter p(e);
  style().draw(p,this,  Style::E_MenuItemBackground,state(),Rect(0,0,w(),h()),Style::Extra(*this));
  style().draw(p,text(),Style::TE_ButtonTitle,      state(),Rect(0,0,w(),h()),Style::Extra(*this));
  }

void Menu::MenuPanel::paintEvent(PaintEvent &e) {
  Tempest::Painter p(e);
  style().draw(p,this,Style::E_MenuBackground,state(),Rect(0,0,w(),h()),Style::Extra(*this));
  }
