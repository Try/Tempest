#pragma once

#include <algorithm>

namespace Tempest {
namespace Detail {

template<class T>
class ComPtr final {
  public:
    ComPtr()=default;
    explicit ComPtr(T* t) :p(t){}
    ComPtr(const ComPtr& t) = delete;
    ComPtr(ComPtr&& other):p(other.p){ other.p=nullptr; }
    ~ComPtr(){ if(p!=nullptr) p->Release(); }

    ComPtr& operator = (const ComPtr&)=delete;
    ComPtr& operator = (ComPtr&& other) { std::swap(other.p,p); return *this; }

    T* operator -> () { return  p; }
    T& operator *  () { return *p; }
    T*& get() { return p; }

  private:
    T* p=nullptr;
  };

}
}
