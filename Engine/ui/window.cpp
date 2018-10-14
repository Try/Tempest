#include "window.h"

using namespace Tempest;

Window::Window()
  :impl(this) {
  id = Tempest::SystemApi::createWindow(&impl,800,600);
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

void Window::resizEvent(uint32_t /*w*/, uint32_t /*h*/) {
  }
