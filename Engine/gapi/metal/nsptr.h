#pragma once

#include <algorithm>

namespace Tempest {
namespace Detail {

class NsIdPtr {
  protected:
    NsIdPtr() = default;
    NsIdPtr(void* ptr);
    ~NsIdPtr();

    void* ptr = nullptr;
  };

template<class T>
class NsPtr final : public NsIdPtr {
  public:
    NsPtr()=default;
    explicit NsPtr(T* t) :p(t){}
    NsPtr(const NsPtr& t) = delete;
    NsPtr(NsPtr&& other):p(other.p){ other.p=nullptr; }
    ~NsPtr();

    NsPtr& operator = (const NsPtr&)=delete;
    NsPtr& operator = (NsPtr&& other) { std::swap(other.p,p); return *this; }
    NsPtr& operator = (std::nullptr_t) { if(p!=nullptr) p->Release(); p = nullptr; return *this; }

    T* operator -> () { return  p; }
    T& operator *  () { return *p; }
    T*& get() { return p; }
    T*  get() const { return p; }

  private:
    T* p=nullptr;
  };

}
}
