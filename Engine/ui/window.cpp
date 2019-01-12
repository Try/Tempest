#include "window.h"

#include <Tempest/VectorImage>

using namespace Tempest;

Window::Window()
  :impl(this) {
  id = Tempest::SystemApi::createWindow(&impl,800,600);
  setGeometry(SystemApi::windowClientRect(id));
  update();
  }

Window::Window(Window::ShowMode sm)
  :impl(this) {
  id = Tempest::SystemApi::createWindow(&impl,SystemApi::ShowMode(sm));
  setGeometry(SystemApi::windowClientRect(id));
  update();
  }

Window::~Window() {
  Tempest::SystemApi::destroyWindow(id);
  }

int Window::w() const {
  return int(Tempest::SystemApi::width(id));
  }

int Window::h() const {
  return int(Tempest::SystemApi::height(id));
  }

void Window::render() {
  }

void Window::dispatchPaintEvent(VectorImage &surface,TextureAtlas& ta) {
  surface.clear();

  PaintEvent p(surface,ta,this->w(),this->h());
  Widget::dispatchPaintEvent(p);
  }
