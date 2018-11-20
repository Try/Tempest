#include "window.h"

#include <Tempest/VectorImage>

using namespace Tempest;

Window::Window()
  :impl(this) {
  id = Tempest::SystemApi::createWindow(&impl,800,600);
  setGeometry(SystemApi::windowClientRect(id));
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

void Window::resizeEvent(uint32_t /*w*/, uint32_t /*h*/) {
  }

void Window::dispatchPaintEvent(VectorImage &surface) {
  surface.clear();

  PaintEvent p(surface,this->w(),this->h());
  Widget::dispatchPaintEvent(p);
  }
