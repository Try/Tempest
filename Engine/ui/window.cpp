#include "window.h"

#include <Tempest/VectorImage>
#include <Tempest/Except>

using namespace Tempest;

Window::Window() {
  id = Tempest::SystemApi::createWindow(this,800,600);
  if(id==nullptr)
    throw std::system_error(Tempest::SystemErrc::UnableToCreateWindow);
  setGeometry(SystemApi::windowClientRect(id));
  setFocus(true);
  update();
  }

Window::Window(Window::ShowMode sm) {
  id = Tempest::SystemApi::createWindow(this,SystemApi::ShowMode(sm));
  if(id==nullptr)
    throw std::system_error(Tempest::SystemErrc::UnableToCreateWindow);
  setGeometry(SystemApi::windowClientRect(id));
  setFocus(true);
  update();
  }

Window::~Window() {
  Tempest::SystemApi::destroyWindow(id);
  }

void Window::render() {
  }

void Window::dispatchPaintEvent(VectorImage &surface,TextureAtlas& ta) {
  surface.clear();

  PaintEvent p(surface,ta,this->w(),this->h());
  this->astate.needToUpdate = false;
  Widget::dispatchPaintEvent(p);

  SystemApi::dispatchOverlayRender(*this,p);
  }

void Window::closeEvent(CloseEvent& e) {
  e.ignore();
  }

void Window::setCursorPosition(int x, int y) {
  SystemApi::setCursorPosition(hwnd(),x,y);
  }

void Window::setCursorPosition(const Point& p) {
  SystemApi::setCursorPosition(hwnd(),p.x,p.y);
  }

void Window::implShowCursor(CursorShape s) {
  SystemApi::showCursor(hwnd(),s);
  }
