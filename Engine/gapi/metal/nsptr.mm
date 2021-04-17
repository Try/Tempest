#include "nsptr.h"

#include <CoreFoundation/CFBase.h>

using namespace Tempest::Detail;

NsPtr::NsPtr(void *ptr):ptr(ptr) {
  }

NsPtr::NsPtr(NsPtr &&other)
  :ptr(other.ptr) {
  other.ptr = nullptr;
  }

NsPtr &NsPtr::operator =(NsPtr &&other) {
  std::swap(ptr,other.ptr);
  return *this;
  }

NsPtr::~NsPtr() {
  if(ptr==nullptr)
    return;
#ifndef NDEBUG
  //int cnt = CFGetRetainCount(ptr);
  //assert(cnt==1);
#endif
  CFRelease(ptr);
  }
