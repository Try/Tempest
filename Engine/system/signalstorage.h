#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <cstddef>

namespace Tempest {
namespace Detail  {

template<class IT>
class SgStorage {
  private:
    struct Data {
      uint32_t refcount=1;
      uint32_t size    =0;
      uint8_t  byte[1] ={};

      void addRef(){ refcount++; }
      void decRef(){
        refcount--;
        if(refcount==0)
          std::free(this);
        }
      };

    enum contants:size_t {
      sizeOfData=sizeof(Data)-1
      };

  public:
    SgStorage(){}
    ~SgStorage(){
      if(data!=nullptr)
        data->decRef();
      }
    SgStorage(const SgStorage& other):data(other.data){
      if(data!=nullptr)
        data->addRef();
      }
    SgStorage(SgStorage&& other):data(other.data){
      other.data=nullptr;
      }
    SgStorage& operator=(const SgStorage&)=delete;
    SgStorage& operator=(SgStorage&& other){
      std::swap(other.data,data);
      return *this;
      }

    template<class T,class...Args>
    void push(Args...args) {
      static_assert(alignof(T)==alignof(std::max_align_t),"bad aligment");
      static_assert(std::is_trivially_copyable<T>::value,""); //for realloc

      if(data==nullptr) {
        data=reinterpret_cast<Data*>(std::malloc(sizeOfData+sizeof(T)));
        if(data==nullptr)
          throw std::bad_alloc();
        new(data) Data();
        data->size = sizeof(T);
        new(data->byte) T(args...);
        } else {
        const size_t nsize=data->size+sizeof(T);
        data = implRealloc(data,nsize);
        new(data->byte+data->size) T(args...);
        data->size=nsize;
        }
      }

    void erase(const IT* at,size_t sz) {
      uintptr_t begin=uintptr_t(&data->byte[0]);
      uintptr_t pos  =uintptr_t(at)-begin;

      if(data->refcount==1) {
        std::memmove(&data->byte[pos],&data->byte[pos+sz],data->size-pos-sz);
        data->size-=sz;
        Data* nd=reinterpret_cast<Data*>(std::realloc(data,sizeOfData+data->size));
        if(nd!=nullptr)
          data=nd;
        } else {
        data->refcount--;
        char* ptr=reinterpret_cast<char*>(std::malloc(sizeOfData+data->size-sz));
        if(ptr==nullptr)
          throw std::bad_alloc();
        char* src=reinterpret_cast<char*>(data);
        std::memcpy(ptr,                src,                   sizeOfData+pos);
        std::memcpy(ptr+sizeOfData+pos, src+sizeOfData+pos+sz, data->size-pos-sz);
        data = reinterpret_cast<Data*>(ptr);
        data->refcount=1;
        data->size-=sz;
        }
      }

    const IT* begin() const {
      if(data!=nullptr)
        return reinterpret_cast<const IT*>(&data->byte[0]); else
        return nullptr;
      }

    const IT* end() const {
      if(data!=nullptr)
        return reinterpret_cast<const IT*>(&data->byte[0]+data->size); else
        return nullptr;
      }

  private:
    Data* data=nullptr;

    Data* implRealloc(Data* d,size_t nsize){
      Data* ndata;
      if(d->refcount>1){
        d->refcount--;
        ndata=reinterpret_cast<Data*>(std::malloc(sizeOfData+nsize));
        if(ndata==nullptr)
          throw std::bad_alloc();
        std::memcpy(ndata,d,sizeOfData+d->size);
        ndata->refcount=1;
        } else {
        ndata=reinterpret_cast<Data*>(std::realloc(d,sizeOfData+nsize));
        if(ndata==nullptr)
          throw std::bad_alloc();
        }
      return ndata;
      }
  };

}
}
