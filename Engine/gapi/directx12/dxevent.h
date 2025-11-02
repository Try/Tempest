#pragma once

#include <windows.h>

namespace Tempest {
namespace Detail {

class DxEvent {
  public:
    DxEvent();
    explicit DxEvent(bool state);
    ~DxEvent();

    DxEvent(DxEvent&& other);
    DxEvent& operator = (DxEvent&& other);

    HANDLE hevt = nullptr;
  };

}
}
