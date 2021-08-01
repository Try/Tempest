#include "dialog.h"

#include <Tempest/Application>
#include <Tempest/Layout>
#include <Tempest/Painter>
#include <Tempest/UiOverlay>

using namespace Tempest;

struct Dialog::LayShadow : Tempest::Layout {
  bool hasLay = false;

  void applyLayout(){
    Widget& ow    = *owner();
    size_t  count = ow.widgetsCount();

    if(hasLay || ow.size().isEmpty())
      return;

    const bool sendShowEvent = !hasLay;
    hasLay = true;

    for(size_t i=0;i<count;++i){
      Widget& w = ow.widget(i);

      if(sendShowEvent) {
        if(auto d = dynamic_cast<Dialog*>(&w))
          d->showEvent();
        }

      Point pos = w.pos();
      if(w.x()+w.w()>ow.w())
        pos.x = ow.w()-w.w();
      if(w.y()+w.h()>ow.h())
        pos.y = ow.h()-w.h();
      if(pos.x<=0)
        pos.x = 0;
      if(pos.y<=0)
        pos.y = 0;

      w.setPosition(pos);
      }
    }
  };

struct Dialog::Overlay : public Tempest::UiOverlay {
  Dialog& dlg;

  Overlay(Dialog& dlg):dlg(dlg){}

  void mouseDownEvent(MouseEvent& e) override {
    if(dlg.modal)
      e.accept(); else
      e.ignore();
    if(dlg.popup)
      dlg.close();
    }

  void mouseMoveEvent(MouseEvent& e) override {
    e.accept();
    }

  void mouseWheelEvent(MouseEvent& e) override {
    if(dlg.modal)
      e.accept(); else
      e.ignore();
    }

  void paintEvent(PaintEvent& e) override {
    dlg.paintShadow(e);
    }

  void keyDownEvent(Tempest::KeyEvent& e) override {
    dlg.keyDownEvent(e);
    e.accept();
    }

  void keyUpEvent(Tempest::KeyEvent& e) override {
    dlg.keyUpEvent(e);
    e.accept();
    }

  void closeEvent(Tempest::CloseEvent& e) override {
    dlg.closeEvent(e);
    }
  };

Dialog::Dialog() {
  resize(300,200);
  }

Dialog::~Dialog() {
  close();
  }

int Dialog::exec() {
  if(owner_ov==nullptr){
    owner_ov = new Overlay(*this);

    SystemApi::addOverlay(std::move(owner_ov));
    owner_ov->setLayout(new LayShadow());
    owner_ov->addWidget(this);
    }

  setVisible(true);
  showEvent();
  while(owner_ov && Application::isRunning()) {
    Application::processEvents();
    }
  return 0;
  }

void Dialog::close() {
  if(owner_ov==nullptr)
    return;
  Tempest::UiOverlay* ov = owner_ov;
  owner_ov = nullptr;

  setVisible(false);
  CloseEvent e;
  this->closeEvent(e);

  ov->takeWidget(this);
  delete ov;
  }

bool Dialog::isOpen() const {
  return owner_ov!=nullptr;
  }

void Dialog::setModal(bool m) {
  modal = m;
  }

bool Dialog::isModal() const {
  return modal;
  }

void Dialog::setPopup(bool p) {
  popup = p;
  }

bool Dialog::isPopup() const {
  return popup;
  }

void Dialog::closeEvent(CloseEvent&) {
  if(owner_ov!=nullptr)
    close();
  }

void Dialog::keyDownEvent(KeyEvent &e) {
  if(e.key==KeyEvent::K_ESCAPE)
    close();
  }

void Dialog::keyUpEvent(KeyEvent& e) {
  e.ignore();
  }

void Dialog::paintEvent(PaintEvent& e) {
  Tempest::Painter p(e);
  style().draw(p,this,Style::E_Background,state(),Rect(0,0,w(),h()),Style::Extra(*this));
  }

void Dialog::paintShadow(PaintEvent &e) {
  if(owner_ov==nullptr)
    return;
  Painter p(e);
  style().draw(p,this,owner_ov,Style::E_Background,state(),
               rect(),Style::Extra(*this),Rect(0,0,owner_ov->w(),owner_ov->h()));
  }

void Dialog::showEvent() {
  auto& ow = *owner();
  Point pos;
  pos.x = (ow.w()-this->w())/2;
  pos.y = (ow.h()-this->h())/2;
  setPosition(pos);
  }
