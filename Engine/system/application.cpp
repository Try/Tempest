#include "application.h"

#include <Tempest/SystemApi>
#include <Tempest/Timer>

#include <thread>

using namespace Tempest;

struct Application::Impl : SystemApi::AppCallBack {
  static std::vector<Timer*> timer;
  static size_t              timerI;

  static void addTimer(Timer& t){
    timer.push_back(&t);
    }

  static void delTimer(Timer& t){
    for(size_t i=0;i<timer.size();++i)
      if(timer[i]==&t){
        timer.erase(timer.begin()+int(i));
        if(timerI>=i)
          timerI--;
        return;
        }
    }

  uint32_t onTimer() override {
    auto now = Application::tickCount();
    size_t count=0;
    for(timerI=0;timerI<timer.size();++timerI){
      Timer& t = *timer[timerI];
      if(t.process(now))
        count++;
      }
    return count;
    }
  };

std::vector<Timer*> Application::Impl::timer;
size_t              Application::Impl::timerI=size_t(-1);

Application::Application()
  :impl(new Impl()){
  }

Application::~Application(){
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
  return SystemApi::exec(*impl);
  }

void Application::implAddTimer(Timer &t) {
  Impl::addTimer(t);
  }

void Application::implDelTimer(Timer &t) {
  Impl::delTimer(t);
  }
