#pragma once

#include <utility>

namespace Tempest {
namespace Detail {

template<class Handler>
class DPtr {
  public:
    DPtr()=default;
    DPtr(const DPtr&)=delete;
    DPtr(Handler h):handler(h){}
    DPtr(DPtr&& other) noexcept :handler(other.handler){ other.handler=Handler();}
    ~DPtr(){}

    DPtr& operator = (DPtr&& other) noexcept { std::swap(handler,other.handler); }

    bool operator !() const { return !handler; }
    operator bool() const { return bool(handler); }

    Handler handler{};
  };

}
}
