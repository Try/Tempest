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

  template<class T, size_t sz>
  class SmallList {
    public:
      struct Node {
        Node* next    = nullptr;
        T     val[sz] = {};
        };

      constexpr static size_t chunkSize = sz;

      SmallList() {
        back = &root;
        }

      ~SmallList() {
        clear();
        }

      void clear() {
        auto n = root.next;
        while(n!=nullptr) {
          auto nx = n->next;
          delete n;
          n = nx;
          }
        backSz = 0;
        allSz  = 0;
        }

      void push(const T& t) {
        if(backSz==sz) {
          back->next = new Node();
          back       = back->next;
          backSz     = 0;
          }
        back->val[backSz] = t;
        ++backSz;
        ++allSz;
        }

      const size_t size() const { return allSz; }
      const Node* begin() { return &root; }

    private:
      Node   root;
      Node*  back   = nullptr;
      size_t backSz = 0;
      size_t allSz  = 0;
    };
}
}
