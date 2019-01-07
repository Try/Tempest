#include "application.h"

#include <Tempest/SystemApi>
#include <thread>

using namespace Tempest;

Application::Application() { 
  }

void Application::sleep(unsigned int msec) {
  std::this_thread::sleep_for(std::chrono::milliseconds(msec));
  }

uint64_t Application::tickCount() {
  auto now = std::chrono::steady_clock::now();
  auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
  return uint64_t(ms.time_since_epoch().count())-uint64_t(INT64_MIN);
  }

int Application::exec(){
  return SystemApi::exec();
  }
