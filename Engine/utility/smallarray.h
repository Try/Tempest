#pragma once

namespace Tempest {
namespace Detail {

  template<class T, size_t sz>
  class SmallArray {
    public:
      SmallArray(size_t n) {
        if(n>=sz) {
          ptr = new T[n];
          }
        }

      ~SmallArray() {
        if(ptr!=stk)
          delete[] ptr;
        }

      SmallArray(const SmallArray&) = delete;
      SmallArray(SmallArray&&) = delete;

            T& operator[](size_t i)       { return ptr[i]; }
      const T& operator[](size_t i) const { return ptr[i]; }

            T* get()       { return ptr; }
      const T* get() const { return ptr; }

    private:
      T  stk[sz] = {};
      T* ptr     = stk;
    };
}
}
