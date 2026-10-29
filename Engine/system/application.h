#pragma once

#include <Tempest/Signal>
#include <Tempest/GameController>

#include <cstdint>
#include <memory>
#include <vector>

namespace Tempest {

class Timer;
class Style;
class Font;

class Application {
  public:
    Application();
    Application(const Application&)=delete;
    virtual ~Application();

    static void         sleep(uint32_t msec);
    static uint64_t     tickCount();

    int                 exec();
    static bool         isRunning();
    static void         processEvents();
    // macOS: main thread, after NSApplication launch, with its run loop running.
    static std::vector<GameController> controllers();

    // UI-thread boundary between native event-processing steps. May re-enter
    // through a modal loop; subscribers must unbind before being destroyed.
    static Signal<void()>& onEventLoop();

    static void         setStyle(const Style* stl);
    static const Style& style();

    static void         setFont(const Font& fnt);
    static const Font&  font();
    static const Font&  defaultFont();

  private:
    struct Impl;
    static Impl impl;

    static void     implAddTimer(Timer& t);
    static void     implDelTimer(Timer& t);

  friend class Timer;
  };

}
