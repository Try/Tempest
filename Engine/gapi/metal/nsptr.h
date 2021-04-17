#pragma once

#include <algorithm>

namespace Tempest {
namespace Detail {

class NsPtr {
  public:
    NsPtr() = default;
    explicit NsPtr(void* ptr);
    NsPtr(NsPtr&& other);
    NsPtr& operator = (NsPtr&& other);
    ~NsPtr();

    id get() const   { return  (__bridge id)(ptr); }

    void* ptr = nullptr;
  };

template<class T, class U>
static inline id get(U* ptr) {
  return reinterpret_cast<T*>(ptr)->impl.get();
  }

}
}
