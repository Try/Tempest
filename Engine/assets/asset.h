#pragma once

#include <memory>
#include <typeinfo>

namespace Tempest {

class Asset {
  public:
    Asset()=default;

    template<class T>
    const T& get() const {
      auto p=impl->get(typeid(T));
      if(p!=nullptr)
        return *reinterpret_cast<T*>(p);

      static T empty;
      return empty;
      }

  private:
    struct Impl {
      virtual ~Impl()=default;
      virtual void* get(const std::type_info& t)=0;
      };

    Asset(Impl* i):impl(i){}

    std::shared_ptr<Impl> impl;

  friend class Assets;
  };

}
