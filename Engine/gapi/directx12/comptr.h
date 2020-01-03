#pragma once

namespace Tempest {
namespace Detail {

template<class T>
class ComPtr final {
  public:
    ComPtr()=default;
    explicit ComPtr(T* t) :p(t){}
    ComPtr(const ComPtr& t) = delete;
    ~ComPtr(){ if(p!=nullptr) p->Release(); }

    ComPtr& operator = (const ComPtr&)=delete;

    T* operator -> () { return  p; }
    T& operator *  () { return *p; }
    T*& get() { return p; }

  private:
    T* p=nullptr;
  };

}
}
