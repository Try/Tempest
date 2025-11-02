#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxevent.h"

#include <new>
#include <utility>

using namespace Tempest;
using namespace Tempest::Detail;

DxEvent::DxEvent() {
  }

DxEvent::DxEvent(bool state) {
  hevt = CreateEvent(nullptr, TRUE, state ? TRUE : FALSE, nullptr);
  if(hevt == nullptr)
    throw std::bad_alloc();
  }

DxEvent::~DxEvent() {
  if(hevt!=nullptr)
    CloseHandle(hevt);
  }

DxEvent::DxEvent(DxEvent&& other) {
  std::swap(this->hevt, other.hevt);
  }

DxEvent& DxEvent::operator =(DxEvent&& other) {
  std::swap(this->hevt, other.hevt);
  return *this;
  }

#endif
