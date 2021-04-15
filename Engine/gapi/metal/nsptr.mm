#include "nsptr.h"

#include <CoreFoundation/CFBase.h>

using namespace Tempest::Detail;

NsIdPtr::NsIdPtr(void *ptr):ptr(ptr) {
  if(ptr!=nullptr)
    CFRetain(ptr);
  }

NsIdPtr::~NsIdPtr() {
  if(ptr!=nullptr)
    CFRelease(ptr);
  }
