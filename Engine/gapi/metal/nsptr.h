#pragma once

#include <algorithm>

namespace Tempest {
namespace Detail {

template<class T>
class NsPtr final {
  public:
    NsPtr()=default;
    explicit NsPtr(T* t) :p(t){}
    NsPtr(const NsPtr& t) = delete;
    NsPtr(NsPtr&& other):p(other.p){ other.p=nullptr; }
    ~NsPtr(){ if(p!=nullptr) p->release(); }

    static NsPtr<T> init() {
      auto p = T::alloc();
      if(p==nullptr)
        return NsPtr<T>();
      return NsPtr<T>(p->init());
      }

    NsPtr& operator = (const NsPtr&)=delete;
    NsPtr& operator = (NsPtr&& other) { std::swap(other.p,p); return *this; }
    NsPtr& operator = (std::nullptr_t) { if(p!=nullptr) p->release(); p = nullptr; return *this; }

    T* operator -> () { return  p; }
    T& operator *  () { return *p; }
    T*& get() { return p; }
    T*  get() const { return p; }

    bool operator == (std::nullptr_t) const { return p==nullptr; }
    bool operator != (std::nullptr_t) const { return p!=nullptr; }

  private:
    T* p=nullptr;
  };

}
}
