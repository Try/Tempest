#pragma once

#include "../gamepad.h"

#include <memory>

namespace Tempest::Detail {

// Main-thread GameController adapter. Window pointers are borrowed NSWindows.
class MacOSGamepad final {
  public:
    MacOSGamepad();
    ~MacOSGamepad();
    MacOSGamepad(const MacOSGamepad&) = delete;
    MacOSGamepad& operator=(const MacOSGamepad&) = delete;

    void addWindow(void* window);
    void removeWindow(void* window);
    std::vector<Gamepad::Device> devices() const;
    bool state(Gamepad::DeviceId id, Gamepad::State& out) const;
    bool pollEvent(Gamepad::Event& out);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
  };

}
