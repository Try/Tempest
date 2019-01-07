#pragma once

#include <cstdint>

namespace Tempest {

class Application {
  public:
    Application();
    Application(const Application&)=delete;

    static void     sleep(unsigned int msec);
    static uint64_t tickCount();

    int             exec();
  };

}
