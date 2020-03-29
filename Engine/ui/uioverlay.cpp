#include "uioverlay.h"

#include <Tempest/Window>

using namespace Tempest;

UiOverlay::UiOverlay() {
  }

UiOverlay::~UiOverlay() {
  SystemApi::takeOverlay(this);
  }

bool UiOverlay::bind(Widget& w) {
  if(auto wx = dynamic_cast<Window*>(&w))
    return bind(*wx);
  return false;
  }

bool UiOverlay::bind(Window& w) {
  if(owner==&w) {
    setGeometry(w.rect());
    return true;
    }
  if(owner!=nullptr)
    return false;
  owner = &w;
  setGeometry(w.rect());
  applyLayout();
  return true;
  }

void UiOverlay::dispatchDestroyWindow(SystemApi::Window* w) {
  if(owner->hwnd()==w)
    owner = nullptr;
  }

