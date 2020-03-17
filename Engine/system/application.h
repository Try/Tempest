#pragma once

#include <cstdint>
#include <memory>

namespace Tempest {

class Timer;
class Style;

class Application {
  public:
    Application();
    Application(const Application&)=delete;
    virtual ~Application();

    static void         sleep(unsigned int msec);
    static uint64_t     tickCount();

    int                 exec();

    static void         setStyle(const Style* stl);
    static const Style& style();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    static void     implAddTimer(Timer& t);
    static void     implDelTimer(Timer& t);

  friend class Timer;
  };

}
