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

    template<class T,class Ret,class ... TArgs>
    void bind(T* obj,Ret (T::*fn)(TArgs...a)) {
      using Check=decltype((std::declval<T>().*fn)(std::declval<Args>()...)); // beautify compiller error message
      static_assert(std::is_same<Check,Ret>::value,""); // unused type warning
      storage.template push<TImpl<T,Ret,TArgs...>>(obj,fn);
      }

    template<class T,class Ret,class ... TArgs>
    void bind(T* obj,Ret (T::*fn)(TArgs...a) const) {
      using Check=decltype((std::declval<T>().*fn)(std::declval<Args>()...)); // beautify compiller error message
      static_assert(std::is_same<Check,Ret>::value,""); // unused type warning
      storage.template push<TImplConst<T,Ret,TArgs...>>(obj,fn);
      }

    template<class T,class Ret,class ... TArgs>
    void ubind(T* obj,Ret (T::*fn)(TArgs...a)) {
      auto b = storage.begin();
      auto e = storage.end();

      TImpl<T,Ret,TArgs...> ref(obj,fn);
      for(auto i=b;i!=e;i=i->next()){
        if(i->is(ref.tag,&ref)) {
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
    enum TypeTag:uint8_t {
      T_MemFn,
      T_MemFnConst
      };

    struct Impl { // force maximum aligment to emplace Impl's into SignalStorage
      virtual ~Impl(){}
      virtual void        call(Args&... a) const=0;
      virtual const Impl* next() const=0;

      virtual bool is(TypeTag type,void* ob) const = 0;
      };

    template<class T,class Ret,class ... TArgs>
    struct TImpl : Impl {
      static constexpr const TypeTag tag=T_MemFn;
      union {
        T*  obj;
        std::max_align_t align;
        };
      Ret (T::*fn)(TArgs...a);

      TImpl(T* obj,Ret (T::*fn)(TArgs...a)):obj(obj),fn(fn){}

      bool operator==(const TImpl& other) const {
        return obj==other.obj && fn==other.fn;
        }

      void call(Args&... a) const override {
        (obj->*fn)(a...);
        }

      const Impl* next() const override {
        return reinterpret_cast<const Impl*>(reinterpret_cast<const char*>(this)+sizeof(*this));
        }

      bool is(TypeTag type,void* ob) const override {
        return type==tag && *this==*reinterpret_cast<TImpl*>(ob);
        }
      };

    template<class T,class Ret,class ... TArgs>
    struct TImplConst : Impl {
      static constexpr const TypeTag tag=T_MemFnConst;
      union {
        T*  obj;
        std::max_align_t align;
        };
      Ret (T::*fn)(TArgs...a) const;

      TImplConst(T* obj,Ret (T::*fn)(TArgs...a) const ):obj(obj),fn(fn){}

      bool operator==(const TImplConst& other) const {
        return obj==other.obj && fn==other.fn;
        }

      void call(Args&... a) const override {
        (obj->*fn)(a...);
        }

      const Impl* next() const override {
        return reinterpret_cast<const Impl*>(reinterpret_cast<const char*>(this)+sizeof(*this));
        }

      bool is(TypeTag type,void* ob) const override {
        return type==tag && *this==*reinterpret_cast<TImplConst*>(ob);
        }
      };

    Detail::SgStorage<Impl> storage;
  };
}
