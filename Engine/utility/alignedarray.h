#pragma once

#include <cstdint>
#include <type_traits>
#include <stdexcept>
#include <memory>

namespace Tempest {

template<class T>
class AlignedArray final {
  public:
    static_assert(std::is_trivially_copyable<T>::value,"T must be trivial");

    AlignedArray(const size_t align);
    AlignedArray(AlignedArray&& other);
    ~AlignedArray();

    AlignedArray& operator=(AlignedArray&& other);

    size_t   size()        const { return sz; }
    size_t   byteSize()    const { return sz*alignedSize; }
    size_t   elementSize() const { return alignedSize; }
    void     resize(size_t sz);

          T& operator[](size_t i);
    const T& operator[](size_t i) const;

    const void* data() const { return buf; }

  private:
    void*  buf=nullptr;
    T&     get(void* ptr,size_t i) { return *reinterpret_cast<T*>(reinterpret_cast<char*>(ptr)+i*alignedSize); }

    size_t sz =0;
    size_t alignedSize=1;
  };

template<class T>
AlignedArray<T>::AlignedArray(const size_t align)
  :alignedSize(((sizeof(T)+align-1)/align)*align){
  }

template<class T>
AlignedArray<T>::AlignedArray(AlignedArray&& other)
  :buf(other.buf),sz(other.sz),alignedSize(other.alignedSize){
  other.buf=nullptr;
  other.sz =0;
  }

template<class T>
AlignedArray<T>::~AlignedArray(){
  while(sz>0) {
    --sz;
    get(buf,sz).~T();
    }
  std::free(buf);
  }

template<class T>
AlignedArray<T>& AlignedArray<T>::operator=(AlignedArray&& other) {
  std::swap(buf,         other.buf);
  std::swap(sz,          other.sz);
  std::swap(buf,         other.buf);
  std::swap(alignedSize, other.alignedSize);
  }

template<class T>
void AlignedArray<T>::resize(size_t size){
  if(sz<size) {
    void* n = std::realloc(buf,size*alignedSize);
    if(n==nullptr)
      throw std::bad_alloc();
    buf=n;

    size_t i=sz;
    try {
      for(;i<size;++i)
        new(&get(n,i)) T();
      }
    catch(...){
      while(i>sz) {
        --i;
        get(n,i).~T();
        }
      throw;
      }

    sz=size;
    }
  else if(sz>size){
    for(size_t i=sz;i>size;){
      --i;
      get(buf,i).~T();
      }
    void* n = std::realloc(buf,size*alignedSize);
    if(n!=nullptr)
      buf=n;
    sz=size;
    }
  }

template<class T>
T& AlignedArray<T>::operator[](size_t i){
  return get(buf,i);
  }

template<class T>
const T& AlignedArray<T>::operator[](size_t i) const {
  return *reinterpret_cast<T*>(reinterpret_cast<char*>(buf)+i*alignedSize);
  }
}

