#include "window.h"

#include <Tempest/VectorImage>

using namespace Tempest;

Window::Window() {
  id = Tempest::SystemApi::createWindow(this,800,600);
  setGeometry(SystemApi::windowClientRect(id));
  setFocus(true);
  update();
  }

Window::Window(Window::ShowMode sm) {
  id = Tempest::SystemApi::createWindow(this,SystemApi::ShowMode(sm));
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
  Widget::dispatchPaintEvent(p);
  }

void Window::closeEvent(CloseEvent& e) {
  e.accept();
  }
