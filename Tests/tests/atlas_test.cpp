#include "../gapi/deviceallocator.h"
#include "../gapi/rectallocator.h"

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest::Detail;

struct TestDevice {
  using DeviceMemory=void*;

  DeviceMemory alloc(uint32_t w,uint32_t h){
    return std::malloc(w*h);
    }

  void free(DeviceMemory m){
    std::free(m);
    }
  };

using Allocator =Tempest::RectAllocator<TestDevice>;
using Allocation=typename Allocator::Allocation;

TEST(main, AtlasAlloator0) {
  TestDevice device;
  Allocator  allocator(device);

  auto s1 = allocator.alloc(128,64);
  auto s2 = allocator.alloc(32,32);

  s2=Allocation();
  s1=Allocation();
  }

TEST(main, AtlasAlloator1) {
  TestDevice device;
  Allocator  allocator(device);

  auto s1 = allocator.alloc(128,64);
  auto s2 = s1;

  s2=Allocation();
  s1=Allocation();
  }

TEST(main, AtlasBlockAlloator0) {
  /*
  Allocator::Block<int> b;
  int* v0 = b.alloc();
  int* v1 = b.alloc();
  b.free(v1);
  b.free(v0);
  */
  }

TEST(main, AtlasBlockAlloator1) {
  /*
  Allocator::Allocator<int> b;
  int* st[64]={};
  for(int i=0;i<64;++i)
    st[i] = b.alloc();

  for(int i=0;i<64;++i)
    b.free(st[i]);
    */
  }
