#pragma once

#include <memory>
#include <typeinfo>

namespace Tempest {

class Asset {
  public:
    Asset()=default;

    template<class T>
    const T& get() const {
      using CleanT=typename std::remove_reference<typename std::remove_cv<T>::type>::type;

      auto p=impl->get(typeid(CleanT));
      if(p!=nullptr)
        return *reinterpret_cast<const T*>(p);

      static T empty;
      return empty;
      }

    bool operator == (const Asset& other) const {
      if(impl!=nullptr)
        return other.impl!=nullptr && impl->path()==other.impl->path();

      return other.impl==nullptr;
      }

    bool operator != (const Asset& other) const {
      return !(*this==other);
      }

  private:
    struct Impl {
#ifdef __WINDOWS__
      using str_path=std::u16string;
#else
      using str_path=std::string;
#endif
      virtual ~Impl()=default;
      virtual const void* get(const std::type_info& t)=0;
      virtual const str_path& path() const = 0;
      };

    static size_t calcHash(Impl& i) noexcept {
      std::hash<Impl::str_path> h;
      return h(i.path());
      }

    Asset(std::shared_ptr<Impl>&& i) noexcept :impl(std::move(i)),hash(calcHash(*impl)){}

    std::shared_ptr<Impl> impl;
    size_t                hash=0;

  friend class Assets;
  };

}
