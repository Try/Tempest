#pragma once

#include <memory>
#include <type_traits>
#include <cstddef>

#include "signalstorage.h"

namespace Tempest {

template<class T>
class Signal;

template<class ... Args>
class Signal<void(Args...args)> {
  public:
    Signal()=default;
    Signal(const Signal&)=delete;
    Signal(Signal&& other)=default;
    Signal& operator=(Signal&& other)=default;

    template<class T,class Base,class Ret,class ... TArgs>
    void bind(T* obj,Ret (Base::*fn)(TArgs...a)) {
      using Check=decltype((std::declval<T>().*fn)(std::declval<Args>()...)); // beautify compiller error message
      static_assert(std::is_same<Check,Ret>::value,""); // unused type warning
      storage.template push<TImpl<T,Base,Ret,TArgs...>>(obj,fn);
      }

    template<class T,class Base,class Ret,class ... TArgs>
    void bind(T* obj,Ret (Base::*fn)(TArgs...a) const) {
      using Check=decltype((std::declval<T>().*fn)(std::declval<Args>()...)); // beautify compiller error message
      static_assert(std::is_same<Check,Ret>::value,""); // unused type warning
      storage.template push<TImplConst<T,Base,Ret,TArgs...>>(obj,fn);
      }

    template<class T,class Base,class Ret,class ... TArgs>
    void ubind(T* obj,Ret (Base::*fn)(TArgs...a)) {
      auto b = storage.begin();
      auto e = storage.end();

      TImpl<T,Ret,TArgs...> ref(obj,fn);
      for(auto i=b;i!=e;i=i->next()){
        if(i->equals(ref)) {
          storage.erase(i,sizeof(ref));
          return;
          }
        }
      }

    void operator()(Args... args) const {
      Detail::SgStorage<Impl> stk(storage);
      auto b = stk.begin();
      auto e = stk.end();

      for(auto i=b;i!=e;i=i->next()){
        i->call(args...);
        }
      }

  private:
    template<class T,class Base,class Ret,class ... TArgs>
    struct TImpl;

    template<class T,class Base,class Ret,class ... TArgs>
    struct TImplConst;

    struct alignas(std::max_align_t) Impl { // force maximum aligment to emplace Impl's into SignalStorage
      struct VTbl {
        void (*call)  (const void* self,Args&... a);
        bool (*equals)(const void* self,const Impl& other);
        size_t size;
        };

      const VTbl* vtbl;

      void call(Args&... a) const {
        vtbl->call(this,a...);
        }

      const Impl* next() const {
        return reinterpret_cast<const Impl*>(reinterpret_cast<const char*>(this)+vtbl->size);
        }

      bool equals(Impl& other) const {
        if(vtbl==other.vtbl){
          return vtbl->equals(this,other);
          }
        return false;
        }
      };

    template<class T,class Base,class Ret,class ... TArgs>
    struct alignas(std::max_align_t) TImpl : Impl {
      T*  obj;
      Ret (Base::*fn)(TArgs...a);

      TImpl(T* obj,Ret (Base::*fn)(TArgs...a)):obj(obj),fn(fn){
        this->vtbl = getVtbl();
        }

      static const typename Impl::VTbl* getVtbl(){
        static const auto t=implVtbl();
        return &t;
        }

      static typename Impl::VTbl implVtbl(){
        typename Impl::VTbl t;
        t.call   = &call;
        t.equals = &equals;
        t.size   = sizeof(TImpl);
        return t;
        }

      static bool equals(const void* self,const Impl& other) {
        auto& a=*reinterpret_cast<const TImpl*>(self);
        auto& b=*reinterpret_cast<const TImpl*>(&other);
        return a.obj==b.obj && a.fn==b.fn;
        }

      static void call(const void* self,Args&... a) {
        auto& ptr=*reinterpret_cast<const TImpl*>(self);
        ((ptr.obj)->*(ptr.fn))(a...);
        }
      };

    template<class T,class Base,class Ret,class ... TArgs>
    struct alignas(std::max_align_t) TImplConst : Impl {
      T* obj;
      Ret (Base::*fn)(TArgs...a) const;

      TImplConst(T* obj,Ret (Base::*fn)(TArgs...a) const):obj(obj),fn(fn){
        this->vtbl = getVtbl();
        }

      static const typename Impl::VTbl* getVtbl(){
        static const auto t=implVtbl();
        return &t;
        }

      static typename Impl::VTbl implVtbl(){
        typename Impl::VTbl t;
        t.call   = &call;
        t.equals = &equals;
        t.size   = sizeof(TImplConst);
        return t;
        }

      static bool equals(const void* self,const Impl& other) {
        auto& a=*reinterpret_cast<const TImplConst*>(self);
        auto& b=*reinterpret_cast<const TImplConst*>(&other);
        return a.obj==b.obj && a.fn==b.fn;
        }

      static void call(const void* self,Args&... a) {
        auto& ptr=*reinterpret_cast<const TImplConst*>(self);
        ((ptr.obj)->*(ptr.fn))(a...);
        }
      };

    Detail::SgStorage<Impl> storage;
  };
}
